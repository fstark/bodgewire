# Paddle Tennis

## What it does

Connect +5 V to `J1`, a TTL monitor to `J2`, the left player's paddle controller to `J3` and the right player's to `J4`. The monitor shows a court: a wall along the top and the bottom, a dashed net down the middle, a paddle near each side edge and each player's score at the top, either side of the net. Turning a controller's knob towards its right stop moves that paddle up, towards its left stop down.

The ball bounces off the top and bottom walls and off the paddles. Where it meets a paddle sets its new angle: the top or bottom quarter of the paddle sends it off steeply, the middle quarters at a shallow angle, up from the upper half and down from the lower half. A rally starts slow, with the ball crossing in about a second and a half, and speeds up after three or four hits. A ball that gets past a paddle disappears and scores a point for the other player; about half a second later it comes back out of that edge, slow, heading for the player who scored.

The first player to 11 wins. Then the paddles disappear, the scores stay, and the ball bounces around all four edges until someone presses Start (`SW1`), which sets both scores to 0 and brings the paddles back.

At power-up the picture locks within half a second. The scores may show any number or odd shapes, and the game may be over or under way: press Start for a new game.
