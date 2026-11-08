const express = require("express");
const crypto = require("node:crypto");

const app = express();
app.disable("x-powered-by");
app.use(express.json({ limit: "8kb" }));

const PORT = process.env.PORT || 10000;
const CLIENT_ID = process.env.ELY_CLIENT_ID;
const CLIENT_SECRET = process.env.ELY_CLIENT_SECRET;
const REDIRECT_URI = process.env.ELY_REDIRECT_URI;
const SCOPES = "account_info minecraft_server_session offline_access";
const pending = new Map();
const SESSION_TTL_MS = 10 * 60 * 1000;

function cleanup() {
  const now = Date.now();
  for (const [id, session] of pending) {
    if (session.expiresAt <= now) pending.delete(id);
  }
}
setInterval(cleanup, 60_000).unref();

function configReady() {
  return Boolean(CLIENT_ID && CLIENT_SECRET && REDIRECT_URI);
}

app.get("/health", (_req, res) => {
  res.status(200).json({ ok: true, configured: configReady() });
});

app.post("/auth/start", (_req, res) => {
  if (!configReady()) {
    return res.status(503).json({ error: "OAuth service is not configured yet." });
  }

  cleanup();
  const sessionId = crypto.randomBytes(32).toString("hex");
  const state = crypto.randomBytes(32).toString("hex");
  pending.set(sessionId, {
    state,
    status: "pending",
    expiresAt: Date.now() + SESSION_TTL_MS
  });

  const url = new URL("https://account.ely.by/oauth2/v1");
  url.search = new URLSearchParams({
    client_id: CLIENT_ID,
    redirect_uri: REDIRECT_URI,
    response_type: "code",
    scope: SCOPES,
    state
  }).toString();

  res.set("Cache-Control", "no-store");
  res.json({ session_id: sessionId, authorization_url: url.toString(), expires_in: 600 });
});

app.get("/oauth/ely", async (req, res) => {
  const { code, state, error } = req.query;
  const session = [...pending.values()].find((item) => item.state === state);

  if (!session) {
    return res.status(400).send("This login session is invalid or expired. Return to KilmorRPG and try again.");
  }

  const entry = [...pending.entries()].find(([, item]) => item === session);
  if (!entry) return res.status(400).send("Login session not found.");
  const [sessionId] = entry;

  if (error) {
    session.status = "error";
    session.error = String(error);
    session.expiresAt = Date.now() + 60_000;
    return res.status(400).send("Ely.by authorization was not completed. You can close this page and return to KilmorRPG.");
  }

  if (typeof code !== "string") {
    return res.status(400).send("Ely.by did not return an authorization code.");
  }

  try {
    const body = new URLSearchParams({
      client_id: CLIENT_ID,
      client_secret: CLIENT_SECRET,
      redirect_uri: REDIRECT_URI,
      grant_type: "authorization_code",
      code
    });

    const tokenResponse = await fetch("https://account.ely.by/api/oauth2/v1/token", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body
    });

    const tokenData = await tokenResponse.json();
    if (!tokenResponse.ok) {
      session.status = "error";
      session.error = "token_exchange_failed";
      session.expiresAt = Date.now() + 60_000;
      console.error("Ely.by token exchange failed:", tokenResponse.status);
      return res.status(502).send("Could not finish Ely.by sign-in. Return to KilmorRPG and try again.");
    }

    session.status = "complete";
    session.tokens = tokenData;
    session.expiresAt = Date.now() + 5 * 60_000;
    res.set("Cache-Control", "no-store");
    res.send("KilmorRPG sign-in completed. You can close this tab and return to the launcher.");
  } catch (err) {
    session.status = "error";
    session.error = "network_error";
    session.expiresAt = Date.now() + 60_000;
    console.error("OAuth callback error:", err.message);
    res.status(502).send("A temporary error occurred. Return to KilmorRPG and try again.");
  }
});

app.get("/auth/result/:sessionId", (req, res) => {
  cleanup();
  const session = pending.get(req.params.sessionId);
  res.set("Cache-Control", "no-store");
  if (!session) return res.status(404).json({ status: "expired" });
  if (session.status === "pending") return res.status(202).json({ status: "pending" });
  if (session.status === "error") {
    pending.delete(req.params.sessionId);
    return res.status(400).json({ status: "error", error: session.error });
  }
  const tokens = session.tokens;
  pending.delete(req.params.sessionId);
  return res.json({ status: "complete", tokens });
});

// Refresh tokens on the server so the Ely.by client secret never ships in the launcher.
app.post("/auth/refresh", async (req, res) => {
  if (!configReady()) {
    return res.status(503).json({ error: "OAuth service is not configured yet." });
  }

  const refreshToken = req.body && req.body.refresh_token;
  if (typeof refreshToken !== "string" || refreshToken.length < 10 || refreshToken.length > 4096) {
    return res.status(400).json({ error: "A valid refresh token is required." });
  }

  try {
    const body = new URLSearchParams({
      client_id: CLIENT_ID,
      client_secret: CLIENT_SECRET,
      grant_type: "refresh_token",
      refresh_token: refreshToken
    });
    const tokenResponse = await fetch("https://account.ely.by/api/oauth2/v1/token", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body
    });
    const tokenData = await tokenResponse.json();

    if (!tokenResponse.ok) {
      console.error("Ely.by refresh failed:", tokenResponse.status);
      return res.status(401).json({ error: "refresh_failed" });
    }

    res.set("Cache-Control", "no-store");
    return res.json({ status: "complete", tokens: tokenData });
  } catch (err) {
    console.error("Ely.by refresh request failed:", err.message);
    return res.status(502).json({ error: "network_error" });
  }
});

app.listen(PORT, "0.0.0.0", () => {
  console.log(`KilmorRPG Ely.by auth bridge listening on port ${PORT}`);
});
