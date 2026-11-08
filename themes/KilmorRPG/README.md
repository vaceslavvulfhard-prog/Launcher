# KilmorRPG Amethyst

This branch contains the first functional UI implementation based on the approved
KilmorRPG mockup. It adds the dark amethyst palette, stone-like controls, a left
navigation panel (Play, News, Profile, Settings), and a large Play action.

The navigation buttons are connected to the launcher's existing actions. The real
instance browser remains in place; the Play buttons launch the selected instance
instead of replacing launcher functionality with a static image.

## Artwork still required

The original KilmorRPG artwork is not yet packaged in this branch. The approved
background asset must be added as:

`themes/KilmorRPG/resources/KilmorRPG_background.jpg`

Once that original asset is committed, the central area can display the approved
scene without baking fake controls into a screenshot. The original mockup's
characters, logo, colors and composition must remain unchanged.

## Install

Copy the complete `KilmorRPG` folder into the launcher's application themes directory
(the directory opened by **Folders → Widget Themes**), then select **KilmorRPG Amethyst**
in appearance settings. Restart the launcher if the theme list was already open.

## Current status

- Functional left navigation and Play controls: implemented.
- Stone/amethyst styling: implemented.
- Original background artwork: pending asset packaging.
- Windows build verification: pending CI run.
