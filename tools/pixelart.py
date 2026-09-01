"""Pixel art authoring toolkit.

Hand-typing ASCII rows produces bad FORMS -- you cannot feel a curve one
character at a time, and every row being one longer than the last is textbook
banding. This draws with real primitives instead, then applies the shading
method actual pixel artists use:

    base shape  ->  highlight offset toward the light  ->  shadow crescent
    away from it  ->  outline last

Light is always upper-left. Ramps are hue-shifted: shadows toward blue/purple,
highlights toward yellow, which is what separates living pixel art from flat
clip art.
"""

TRANSPARENT = "."


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[TRANSPARENT] * w for _ in range(h)]

    def set(self, x, y, ch):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = ch

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.px[y][x]
        return TRANSPARENT

    def rows(self):
        return ["".join(r) for r in self.px]


def ellipse(cx, cy, rx, ry):
    """Filled ellipse as a set of pixels. Half-integer centres are allowed, which
    is how you get symmetric shapes on an even-width canvas."""
    out = set()
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx = (x + 0.5 - cx) / max(rx, 0.001)
            dy = (y + 0.5 - cy) / max(ry, 0.001)
            if dx * dx + dy * dy <= 1.0:
                out.add((x, y))
    return out


def rect(x0, y0, x1, y1):
    return {(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)}


def line(x0, y0, x1, y1, thickness=1):
    out = set()
    steps = max(abs(x1 - x0), abs(y1 - y0), 1)
    for i in range(steps + 1):
        t = i / steps
        cx = round(x0 + (x1 - x0) * t)
        cy = round(y0 + (y1 - y0) * t)
        r = thickness // 2
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                out.add((cx + dx, cy + dy))
    return out


def triangle(p1, p2, p3):
    """Filled triangle by barycentric test. Ears and snouts are triangles, and
    triangles are what make an animal silhouette read."""
    xs = [p1[0], p2[0], p3[0]]
    ys = [p1[1], p2[1], p3[1]]
    out = set()

    def sign(a, b, c):
        return (a[0] - c[0]) * (b[1] - c[1]) - (b[0] - c[0]) * (a[1] - c[1])

    for y in range(int(min(ys)), int(max(ys)) + 1):
        for x in range(int(min(xs)), int(max(xs)) + 1):
            pt = (x + 0.5, y + 0.5)
            d1, d2, d3 = sign(pt, p1, p2), sign(pt, p2, p3), sign(pt, p3, p1)
            neg = (d1 < 0) or (d2 < 0) or (d3 < 0)
            pos = (d1 > 0) or (d2 > 0) or (d3 > 0)
            if not (neg and pos):
                out.add((x, y))
    return out


def tri(dark, mid, light):
    """A three-tone ramp shaped for shade(). Reference sprites at this size use
    three tones and an outline -- five muddies the read."""
    return [dark, dark, mid, light, light]


def shift(pixels, dx, dy):
    return {(x + dx, y + dy) for (x, y) in pixels}


def paint(canvas, pixels, ch):
    for (x, y) in pixels:
        canvas.set(x, y, ch)


def shade(canvas, body, ramp, light=(-1, -1), depth=2):
    """Lights a solid shape with a 5-step ramp (dark -> light).

    Form comes from BANDS along the edges, not from a gradient across the whole
    shape: a dark band on the side facing away from the light, a light band on
    the side facing it, and the base tone holding the middle. Painting a large
    interior region with the highlight tone is what washes a sprite out, and
    shading uniformly along the whole outline is pillow shading.
    """
    lx, ly = light
    paint(canvas, body, ramp[2])

    # Far side: everything not still covered when the shape slides toward the light.
    paint(canvas, body - shift(body, lx * depth, ly * depth), ramp[1])
    paint(canvas, body - shift(body, lx, ly), ramp[0])

    # Near side: the mirror of that, and a one-pixel specular on the very edge.
    paint(canvas, body - shift(body, -lx * depth, -ly * depth), ramp[3])
    spec = body - shift(body, -lx, -ly)
    # Trimmed to the quadrant facing the light so it reads as a highlight rather
    # than a rim running all the way round.
    spec = {(x, y) for (x, y) in spec
            if (x, y) in shift(body, -lx * depth * 2, 0)
            and (x, y) in shift(body, 0, -ly * depth * 2)}
    paint(canvas, spec, ramp[4])


def outline(canvas, ch="#", diagonal=False):
    """Surrounds everything opaque. Applied last so it never gets shaded over."""
    edge = set()
    offsets = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    if diagonal:
        offsets += [(-1, -1), (1, -1), (-1, 1), (1, 1)]
    for y in range(canvas.h):
        for x in range(canvas.w):
            if canvas.get(x, y) == TRANSPARENT:
                if any(canvas.get(x + dx, y + dy) not in (TRANSPARENT, ch)
                       for dx, dy in offsets):
                    edge.add((x, y))
    paint(canvas, edge, ch)


def emit(sprites):
    """Renders sprite definitions to the TOML the game loads."""
    out = []
    for name, canvas in sprites:
        rows = canvas.rows()
        assert all(len(r) == canvas.w for r in rows), name
        out += ["[[sprite]]", f'id = "{name}"',
                f"size = [{canvas.w}, {canvas.h}]", "rows = ["]
        out += [f'  "{r}",' for r in rows]
        out += ["]", ""]
    return "\n".join(out)
