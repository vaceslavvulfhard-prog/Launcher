# KilmorRPG Amethyst theme

This theme is the first integration pass for the approved KilmorRPG launcher mockup.
It applies the dark amethyst palette, translucent surfaces and stone-like purple controls
while retaining the launcher's existing functional widgets and actions.

## Required background asset

Place the approved original background image at:

`themes/KilmorRPG/resources/KilmorRPG_background.jpg`

The stylesheet references it through Qt's `theme:` search path. The image itself is
not duplicated in this source-only theme commit; it must be added from the approved
KilmorRPG artwork before packaging the theme.

## Install

Copy the complete `KilmorRPG` folder into the launcher's application themes directory
(the directory opened by **Folders → Widget Themes**), then select **KilmorRPG Amethyst**
in the appearance settings. Restart the launcher if the theme list was already open.

This pass styles the existing Prism-derived launcher widgets. The full custom home layout
from the mockup (dedicated left navigation panel and custom play/profile composition)
requires a separate UI implementation pass; it is not faked as a static screenshot.
