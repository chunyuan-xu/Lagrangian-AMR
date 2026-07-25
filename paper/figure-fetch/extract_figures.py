import os
import re
import fitz

def extract_figures_and_tables(pdf_path, output_dir):
    """
    Extracts all figures and tables from the PDF and saves them as vector graphics (PDF and SVG)
    in the output directory.
    """
    doc = fitz.open(pdf_path)
    print(f"Opened PDF: {pdf_path}")
    print(f"Total pages: {len(doc)}")
    
    caption_pattern = re.compile(r'^\s*(Figure|Fig\.|Table)\s+(\d+)\s*[:\.]', re.IGNORECASE)
    
    # 1. Identify all captions on all pages
    page_captions = {}
    for page_num in range(len(doc)):
        page = doc[page_num]
        blocks = page.get_text("blocks")
        for b in blocks:
            text = b[4].strip()
            match = caption_pattern.match(text)
            if match:
                cap_type = match.group(1).capitalize()
                # standardizing "Fig." to "Figure"
                if cap_type.startswith("Fig"):
                    cap_type = "Figure"
                cap_num = int(match.group(2))
                rect = fitz.Rect(b[:4])
                
                # Merge consecutive lines of the same caption block
                merged_rect = fitz.Rect(rect)
                for b_other in blocks:
                    if b_other[5] != b[5]:
                        r_other = fitz.Rect(b_other[:4])
                        # If the block is directly below the caption start block (within 25 points)
                        if r_other.y0 >= rect.y1 and r_other.y0 <= rect.y1 + 25:
                            # And it's not a new caption
                            if not caption_pattern.match(b_other[4].strip()):
                                merged_rect.include_rect(r_other)
                
                if page_num not in page_captions:
                    page_captions[page_num] = []
                page_captions[page_num].append({
                    "type": cap_type,
                    "num": cap_num,
                    "rect": merged_rect,
                    "text": text
                })
                
    # Sort captions on each page by vertical coordinate
    for page_num in page_captions:
        page_captions[page_num].sort(key=lambda x: x["rect"].y0)
        
    print("\n--- Detected Figures and Tables ---")
    total_found = 0
    for page_num, caps in sorted(page_captions.items()):
        for cap in caps:
            print(f"Page {page_num+1:2d} | {cap['type']} {cap['num']} at {cap['rect']}")
            total_found += 1
    print(f"Total detected: {total_found}")
    
    # 2. Crop and export each figure and table
    os.makedirs(output_dir, exist_ok=True)
    
    for page_num, caps in sorted(page_captions.items()):
        page = doc[page_num]
        blocks = page.get_text("blocks")
        drawings = page.get_drawings()
        images = page.get_image_info(xrefs=True)
        
        for cap in caps:
            # Determine vertical upper bound (y_limit_top)
            y_limit_top = 60.0 # Top margin of the page
            
            # Bound 1: Bottom of any other caption above it on the same page
            for other_cap in caps:
                if other_cap["rect"].y1 < cap["rect"].y0:
                    y_limit_top = max(y_limit_top, other_cap["rect"].y1 + 10)
                    
            # Bound 2: Bottom of any body text block above it on the same page
            for b in blocks:
                b_rect = fitz.Rect(b[:4])
                if b_rect.intersects(cap["rect"]):
                    continue
                if b_rect.y1 < cap["rect"].y0:
                    # Heuristics for body text blocks:
                    # - Starts in left margin range (110 <= x0 <= 135)
                    # - Has significant width (>= 200) or text length (>= 50)
                    is_body = False
                    if 110.0 <= b_rect.x0 <= 135.0 and b_rect.width >= 200:
                        is_body = True
                    elif len(b[4].strip()) >= 50 and 110.0 <= b_rect.x0 <= 135.0:
                        is_body = True
                        
                    if is_body:
                        y_limit_top = max(y_limit_top, b_rect.y1 + 5)
            
            # The search region vertical coordinates
            y_in_min = y_limit_top
            y_in_max = cap["rect"].y0
            
            # Union bounding box of all figure/table elements inside this vertical band
            elements_rect = fitz.Rect()
            
            # 1) Drawings
            for d in drawings:
                d_rect = fitz.Rect(d["rect"])
                if d_rect.y0 >= y_in_min - 5 and d_rect.y1 <= y_in_max + 5:
                    if d_rect.height > 600: # skip page-wide borders/frames
                        continue
                    elements_rect.include_rect(d_rect)
                    
            # 2) Images
            for img in images:
                img_rect = fitz.Rect(img["bbox"])
                if img_rect.y0 >= y_in_min - 5 and img_rect.y1 <= y_in_max + 5:
                    elements_rect.include_rect(img_rect)
                    
            # 3) Text blocks (like labels, legends, table cell values)
            for b in blocks:
                b_rect = fitz.Rect(b[:4])
                if b_rect.intersects(cap["rect"]):
                    continue
                if b_rect.y0 >= y_in_min - 5 and b_rect.y1 <= y_in_max + 5:
                    elements_rect.include_rect(b_rect)
            
            # Compute the final crop box containing elements + caption
            if not elements_rect.is_empty:
                cropbox = fitz.Rect(
                    min(elements_rect.x0, cap["rect"].x0) - 8,
                    elements_rect.y0 - 8,
                    max(elements_rect.x1, cap["rect"].x1) + 8,
                    cap["rect"].y1 + 8
                )
            else:
                # Fallback cropbox if no elements found (uses standard horizontal margins)
                cropbox = fitz.Rect(
                    110 - 8,
                    y_in_min - 8,
                    500 + 8,
                    cap["rect"].y1 + 8
                )
            
            # Apply safety bounds
            cropbox.y0 = max(y_limit_top, cropbox.y0)
            cropbox.x0 = max(0, cropbox.x0)
            cropbox.y0 = max(0, cropbox.y0)
            cropbox.x1 = min(page.mediabox.width, cropbox.x1)
            cropbox.y1 = min(page.mediabox.height, cropbox.y1)
            
            # Define output paths
            prefix = f"{cap['type']}_{cap['num']}"
            pdf_out_path = os.path.join(output_dir, f"{prefix}.pdf")
            svg_out_path = os.path.join(output_dir, f"{prefix}.svg")
            
            print(f"Exporting {cap['type']} {cap['num']} (Page {page_num+1})...")
            
            # 1. Export as PDF
            new_doc = fitz.open()
            new_doc.insert_pdf(doc, from_page=page_num, to_page=page_num)
            new_page = new_doc[0]
            new_page.set_cropbox(cropbox)
            new_doc.save(pdf_out_path)
            new_doc.close()
            
            # 2. Export as SVG
            # Set temporary cropbox on original document page to export correct SVG viewbox
            page.set_cropbox(cropbox)
            try:
                svg_content = page.get_svg_image()
                with open(svg_out_path, "w", encoding="utf-8") as f:
                    f.write(svg_content)
            except Exception as e:
                print(f"  Error exporting SVG for {prefix}: {e}")
            finally:
                # Reset cropbox back to full page dimensions
                page.set_cropbox(page.mediabox)
                
    doc.close()
    print("\nExtraction complete! All vector figures and tables are saved.")

if __name__ == "__main__":
    pdf_file = r"C:\Lagrangian-AMR\paper\figure-fetch\A multi-dimensional finite volume cell-centered direct ALE solver for hydrodynamics.pdf"
    current_directory = os.path.dirname(os.path.abspath(__file__))
    extract_figures_and_tables(pdf_file, current_directory)
