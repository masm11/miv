/*    miv - Masm11's Image Viewer
 *    Copyright (C) 2018-2022  Yuuki Harano
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
