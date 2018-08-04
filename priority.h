#ifndef PRIORITY_H__INCLUDED
#define PRIORITY_H__INCLUDED

// 0: timeouts, X events, event sources, add items slowly
// 110: resize
// 120: redraw
// 200: idle
#define PRIORITY_HIGH		G_PRIORITY_DEFAULT_IDLE		// 200: draw icon in screen
#define PRIORITY_ANIME_TIMER	(G_PRIORITY_DEFAULT_IDLE + 10)	// 210: gif anime timer. should be lower than painter.
#define PRIORITY_MOVIE		(G_PRIORITY_DEFAULT_IDLE + 10)	// 210: movie relayout.
#define PRIORITY_ADD		(G_PRIORITY_DEFAULT_IDLE + 20)	// 220: add items
#define PRIORITY_LOW		(G_PRIORITY_DEFAULT_IDLE + 40)	// 240: draw icon out of screen

#endif	/* ifndef PRIORITY_H__INCLUDED */
