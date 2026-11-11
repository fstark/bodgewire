# TV Typewriter

## What it does

Connect +5 V to `J1`, a TTL monitor to `J2` and an ASCII keyboard to `J3`. The monitor shows a page of 16 lines of 32 characters, upper case, white on black, with a dark border all round. The picture locks within half a second.

At power-up the page is full of random characters and a blinking block sits anywhere on it. Press Clear (`SW1`) to blank the page and put the block at the top left.

Each key typed appears where the block was, and the block moves one place to the right. After 32 characters typing goes on at the start of the next line; Return starts the next line at once. Other control keys (Backspace, Esc, CTRL with a key) do nothing. After the last line typing goes on at the top line, over what is already there: the page never scrolls.
