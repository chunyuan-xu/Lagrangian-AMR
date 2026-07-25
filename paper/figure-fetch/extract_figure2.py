"""
Extract figure2.pdf to a clean SVG, keeping only:
  - The four sub-grids (four quadrants forming the AMR mesh)
  - The parent grid
  - Red hanging points
Removing:
  - All text labels and annotations (English letters, Greek symbols)
  - Geometric annotation markers (arrowheads, leader lines, measurement indicators)
  - Blue and gray annotation elements
"""

import fitz  # PyMuPDF
from lxml import etree

# ── Step 1: Render PDF to SVG ────────────────────────────────────────────────
doc = fitz.open("figure2.pdf")
page = doc[0]
svg_bytes = page.get_svg_image()
doc.close()

root = etree.fromstring(svg_bytes)

# ── Step 2: Colour helpers ───────────────────────────────────────────────────
def parse_color(s):
    """Parse SVG color string to (r,g,b) tuple, or None."""
    if s is None:
        return None
    s = s.strip().lower()
    if s == "none":
        return None
    if s.startswith("#"):
        s = s[1:]
        if len(s) == 3:
            s = "".join(c * 2 for c in s)
        if len(s) == 6:
            return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))
    named = {"red": (255, 0, 0), "blue": (0, 0, 255),
             "white": (255, 255, 255), "black": (0, 0, 0),
             "#ff0000": (255, 0, 0), "#0000ff": (0, 0, 255),
             "#ffffff": (255, 255, 255), "#000000": (0, 0, 0),
             "#bfbfbf": (191, 191, 191)}
    return named.get(s)

BLUE  = {(0, 0, 255)}
GRAY  = {(191, 191, 191)}
WHITE = {(255, 255, 255)}
BLACK = {(0, 0, 0)}
RED   = {(255, 0, 0)}

def get_stroke_width(elem):
    """Return stroke-width as float, or 0."""
    sw = elem.get("stroke-width")
    if sw is None:
        return 0
    try:
        return float(sw)
    except ValueError:
        return 0

def remove_element(elem):
    parent = elem.getparent()
    if parent is not None:
        parent.remove(elem)

# ── Step 3: Keep / remove decision ───────────────────────────────────────────
def should_keep(elem):
    tag = etree.QName(elem).localname

    # Remove <use> elements (all text labels — English, Greek, formulas)
    if tag == "use":
        return False

    # Remove <defs> (font glyphs; only used by the now-removed <use> elements)
    if tag == "defs":
        return False

    if tag != "path":
        return True

    fill_c = parse_color(elem.get("fill"))
    stroke_c = parse_color(elem.get("stroke"))

    # ── Blue / gray annotations → remove ──
    if fill_c in BLUE or stroke_c in BLUE:
        return False
    if fill_c in GRAY or stroke_c in GRAY:
        return False

    # ── White fill → grid cell background → KEEP ──
    if fill_c in WHITE:
        return True

    # ── Red fill → hanging point markers → KEEP ──
    if fill_c in RED:
        return True

    # ── Black stroke elements ──
    if stroke_c in BLACK:
        sw = get_stroke_width(elem)
        # Thin lines (sw < 0.75) are leader lines for text labels → remove
        if 0 < sw < 0.75:
            return False
        # Arrowhead shapes → remove
        d = elem.get("d", "")
        if d.startswith("M5.44046 0"):
            return False
        # Thicker lines are grid/sub-grid lines → KEEP
        if sw >= 0.75:
            return True

    # ── Thin stroked paths of any other colour (e.g. red leader lines) → remove ──
    stroke_val = elem.get("stroke")
    if stroke_val is not None and parse_color(stroke_val) is not None:
        sw = get_stroke_width(elem)
        if 0 < sw < 0.75:
            return False

    # ── Implicit-fill shapes (no fill AND no stroke attr) → arrowheads → remove ──
    if elem.get("fill") is None and elem.get("stroke") is None:
        return False

    # ── Explicit-fill shapes not white/red → annotation markers → remove ──
    if fill_c is not None and fill_c not in WHITE and fill_c not in RED:
        return False

    return True


# ── Step 4: Filter the SVG tree ──────────────────────────────────────────────
for parent in root.iter():
    for child in reversed(list(parent)):
        if not should_keep(child):
            remove_element(child)

# ── Step 5: Output ───────────────────────────────────────────────────────────
svg_str = etree.tostring(root, encoding="unicode", pretty_print=True)
with open("figure2_clean.svg", "w", encoding="utf-8") as f:
    f.write(svg_str)

print(f"Cleaned SVG written to figure2_clean.svg  ({len(svg_str)} bytes)")

kept = list(root.iter())
path_elems = [e for e in kept if etree.QName(e).localname == "path"]
print(f"Kept {len(path_elems)} path elements and {len(kept) - len(path_elems)} other elements")
for p in path_elems:
    d = p.get("d", "")
    fill = p.get("fill", "none")
    stroke = p.get("stroke", "none")
    sw = p.get("stroke-width", "-")
    d_short = d[:60] + "..." if len(d) > 60 else d
    print(f"  {fill:12s} {stroke:12s} sw={sw:8s}  {d_short}")
