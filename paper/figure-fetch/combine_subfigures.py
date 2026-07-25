import os
import fitz

def combine_density_maps():
    pdf_path = r"C:\Lagrangian-AMR\paper\figure-fetch\A multi-dimensional finite volume cell-centered direct ALE solver for hydrodynamics.pdf"
    output_dir = r"C:\Lagrangian-AMR\paper\figure-fetch"
    
    # Open source document
    src_doc = fitz.open(pdf_path)
    print("Opened source paper PDF:", pdf_path)
    
    # Create new document and page
    # Page size: 400 pt width, 260 pt height
    out_doc = fitz.open()
    page_w = 400
    page_h = 260
    page = out_doc.new_page(width=page_w, height=page_h)
    
    # Absolute clip rects from Page 32 (page_num = 31)
    # Perfect square dimensions of 126.72 x 126.72 pt that exclude y-axes, x-axes numbers, and colorbars.
    clip_eul = fitz.Rect(137.76, 516.58, 264.48, 643.3)
    clip_lag = fitz.Rect(137.76, 154.08, 264.48, 280.80)
    
    # Target geometry calculations
    # Both set to identical size 160 x 160 pt, ensuring they are perfectly equal-height, aligned, and un-distorted
    target_size = 160.0
    
    left_center = page_w / 4 # 100
    right_center = (3 * page_w) / 4 # 300
    middle_center = page_w / 2 # 200
    
    # Target Rectangles on the target page (shifted up to y=40 to y=200 for perfect vertical balance)
    rect_eul = fitz.Rect(
        left_center - target_size / 2, # 20
        40,
        left_center + target_size / 2, # 180
        40 + target_size # 200
    )
    
    rect_lag = fitz.Rect(
        right_center - target_size / 2, # 220
        40,
        right_center + target_size / 2, # 380
        40 + target_size # 200
    )
    
    print(f"Eulerian (left) target box: {rect_eul} (width={rect_eul.width:.1f}, height={rect_eul.height:.1f})")
    print(f"Lagrangian (right) target box: {rect_lag} (width={rect_lag.width:.1f}, height={rect_lag.height:.1f})")
    
    # Draw pages onto target canvas (Source page is 31, which is Page 32)
    page.show_pdf_page(rect_eul, src_doc, 31, clip=clip_eul)
    page.show_pdf_page(rect_lag, src_doc, 31, clip=clip_lag)
    
    # --- Write Centered Labels at the Bottom ---
    # We place the labels: "Eulerian", "VS.", "Lagrangian" on the same horizontal line,
    # with the same font (Helvetica-Bold) and size (11pt)
    label_font = "hebo" # Helvetica-Bold
    label_size = 11.5
    y_baseline = 222 # mathematically aligned to leave 38pt margin at the bottom (260 - 222 = 38pt)
    
    # 1. "Eulerian" - centered under left subfigure (x=100)
    eul_txt = "Eulerian"
    eul_w = fitz.get_text_length(eul_txt, fontname=label_font, fontsize=label_size)
    page.insert_text(
        fitz.Point(left_center - eul_w / 2, y_baseline),
        eul_txt,
        fontname=label_font,
        fontsize=label_size,
        color=(0.17, 0.24, 0.31) # Slate charcoal #2C3E50
    )
    
    # 2. "VS." - centered in the middle gap (x=200)
    vs_txt = "VS."
    vs_w = fitz.get_text_length(vs_txt, fontname=label_font, fontsize=label_size)
    page.insert_text(
        fitz.Point(middle_center - vs_w / 2, y_baseline),
        vs_txt,
        fontname=label_font,
        fontsize=label_size,
        color=(0.35, 0.42, 0.49) # Slate gray #5A6B7C
    )
    
    # 3. "Lagrangian" - centered under right subfigure (x=300)
    lag_txt = "Lagrangian"
    lag_w = fitz.get_text_length(lag_txt, fontname=label_font, fontsize=label_size)
    page.insert_text(
        fitz.Point(right_center - lag_w / 2, y_baseline),
        lag_txt,
        fontname=label_font,
        fontsize=label_size,
        color=(0.17, 0.24, 0.31) # Slate charcoal #2C3E50
    )
    
    # Save outputs
    pdf_out = os.path.join(output_dir, "Eulerian_vs_Lagrangian_Perfect.pdf")
    svg_out = os.path.join(output_dir, "Eulerian_vs_Lagrangian_Perfect.svg")
    
    # Save PDF
    out_doc.save(pdf_out)
    out_doc.close()
    src_doc.close()
    print("Saved combined vector PDF to:", pdf_out)
    
    # Save SVG
    new_doc = fitz.open(pdf_out)
    new_page = new_doc[0]
    svg_content = new_page.get_svg_image()
    with open(svg_out, "w", encoding="utf-8") as f:
        f.write(svg_content)
    new_doc.close()
    print("Saved combined vector SVG to:", svg_out)

if __name__ == "__main__":
    combine_density_maps()
