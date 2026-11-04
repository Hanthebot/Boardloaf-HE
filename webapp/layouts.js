// Per-repo layout data.
//
// index.html is IDENTICAL across repos; this is the only file that differs.
// Change HE_DEFAULT_LAYOUT to this repo's board, and list only this repo's
// board(s) in HE_LAYOUTS. Other boards can be loaded at runtime via the
// webapp's "Import Layout" button (persisted in localStorage).
//
// Layout format (same shape as the import/export JSON):
//   id      : unique id (also used as the firmware matrix id)
//   name    : display name
//   rows    : firmware HE_MATRIX_ROWS (muxes per half)
//   cols    : firmware MATRIX_COLS (channels per mux: main columns + thumb col)
//   keymap  : flat array with exactly rows*cols entries; entry i maps to
//             matrix (row = floor(i/cols), col = i%cols). Each entry:
//             { x, y, w?, h?, label?, thumb?, hidden? } in key units (Vial-style).
//             x is mirrored for the right half (reversible board).

window.HE_DEFAULT_LAYOUT = 'boardloaf';

window.HE_LAYOUTS = {
    boardloaf: {
        id: 'boardloaf',
        name: 'Boardloaf-HE',
        rows: 3,
        cols: 6, // 5 main columns + 1 thumb column
        keymap: [
            {x:0,y:0}, {x:1,y:0}, {x:2,y:0}, {x:3,y:0}, {x:4,y:0}, {x:2,y:3.2,thumb:true},
            {x:0,y:1}, {x:1,y:1}, {x:2,y:1}, {x:3,y:1}, {x:4,y:1}, {x:3,y:3.2,thumb:true},
            {x:0,y:2}, {x:1,y:2}, {x:2,y:2}, {x:3,y:2}, {x:4,y:2}, {x:4,y:3.2,thumb:true},
        ],
    },
};
