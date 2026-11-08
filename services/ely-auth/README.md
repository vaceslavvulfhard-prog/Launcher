# KilmorRPG Ely.by OAuth bridge

Small HTTPS callback service for the KilmorRPG desktop launcher. The launcher starts a login with `POST /auth/start`, opens the returned Ely.by authorization URL in the system browser, then polls `GET /auth/result/:session_id` until it receives the token response.

## Render deployment

Create a Render **Web Service** from this repository:
- Branch: `develop`
- Root Directory: `services/ely-auth`
- Build Command: `npm install`
- Start Command: `npm start`
- Instance type: Free (fine for initial testing; it sleeps when idle)

After Render assigns the service hostname, set these environment variables:
- `ELY_CLIENT_ID`: Client ID from Ely.by
- `ELY_CLIENT_SECRET`: Client Secret from Ely.by (keep secret; never commit it)
- `ELY_REDIRECT_URI`: `https://YOUR-SERVICE.onrender.com/oauth/ely`

The exact value of `ELY_REDIRECT_URI` must also be entered in the Ely.by application settings. Do not guess the Render hostname; copy it from the service dashboard.

## API

- `GET /health` — health/configuration check
- `POST /auth/start` — returns `session_id` and `authorization_url`
- `GET /oauth/ely` — Ely.by callback; exchanges the code server-side
- `GET /auth/result/:sessionId` — launcher polls for tokens

The session state and tokens are held in memory for a short time. This is suitable for a single-instance hobby/testing service, not high-availability production. The launcher must treat `session_id` as a secret and must not log or expose it.
