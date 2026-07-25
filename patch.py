import re

with open('src/main.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix 1: MatrixAssemble free
content = re.sub(r'(\t\tquadrant_corner_to_point_matrix_assemble_callback\);\s*/\*.*?\*/\n)}', r'\1\tfree(m_user_data);\n}', content)

# Fix 2: ComputeCornerNodeVelocity free
content = re.sub(r'(\t\tquadrant_corner_velocity_callback\);\s*\n)\n', r'\1\tfree(m_user_data);\n\n', content)

# Fix 3: Matrix Assemble callback my_user_data_t cast
content = re.sub(r'(quad_data_t\s*\*ghost_data = \(quad_data_t\s*\*\)user_data;)', r'my_user_data_t *m_user_data = (my_user_data_t *)user_data;\n\tquad_data_t\t\t*ghost_data = (quad_data_t  *)m_user_data->quad_data;', content, count=1)

# Fix 4: Velocity callback my_user_data_t cast
content = re.sub(r'(quad_data_t\s*\*ghost_data = \(quad_data_t \*\)user_data;)', r'my_user_data_t *m_user_data = (my_user_data_t *)user_data;\n\tquad_data_t\t\t\t\t\t*ghost_data = (quad_data_t *)m_user_data->quad_data;', content, count=1)

# Fix 5: p4est_ghost_exchange_data
content = re.sub(r'(get_boundary_from_p4est\(p4est\);\n)', r'\1\tp4est_ghost_exchange_data(p4est, ghost, ghost_data);\n', content)

# Fix 6: quadrant_relaxed_hanging_solver_callback
content = re.sub(r'(if \(side\[i\]->is.hanging.is_ghost\[0\]\)\s*\{\s*)(m_child1_data)', r'\1if (ghost_data == NULL) continue;\n\t\t\t\t\2', content)
content = re.sub(r'(if \(side\[i\]->is.hanging.is_ghost\[1\]\)\s*\{\s*)(m_child2_data)', r'\1if (ghost_data == NULL) continue;\n\t\t\t\t\2', content)
content = re.sub(r'(if \(side\[full_index\]->is.full.is_ghost\)\s*\{\s*)(m_parent_data)', r'\1if (ghost_data == NULL) continue;\n\t\t\t\t\2', content)

# Fix 7: quadrant_whether_allowing_coarsening_from_edge_callback NULL quad
content = re.sub(r'(p4est_quadrant\s*\*quad_child1 = side\[i\]->is\.hanging\.quad\[0\];\n)', r'\1\t\t\tif (quad_child1 == NULL) continue;\n', content)
content = re.sub(r'(p4est_quadrant\s*\*quad_child2 = side\[i\]->is\.hanging\.quad\[1\];\n)', r'\1\t\t\tif (quad_child2 == NULL) continue;\n', content)
content = re.sub(r'(p4est_quadrant\s*\*quad_parent = \(p4est_quadrant\s*\*\)side\[full_index\]->is\.full\.quad;\n)', r'\1\t\t\tif (quad_parent == NULL) continue;\n', content)
content = re.sub(r'(if \(side\[full_index\]->is.full.is_ghost\)\s*\{\s*)(m_parent_data)', r'\1if (ghost_data == NULL) continue;\n\t\t\t\t\2', content)

# Fix 8: quadrant_whether_allowing_coarsening_from_corner_callback NULL quad
content = re.sub(r'(p4est_quadrant\s*\*quad_a = side\[i\]->quad;\n)', r'\1\t\tif (quad_a == NULL) continue;\n', content)
content = re.sub(r'(p4est_quadrant\s*\*quad_b = side\[j\]->quad;\n)', r'\1\t\t\tif (quad_b == NULL) continue;\n', content)
content = re.sub(r'(if \(is_ghost_a\)\s*\{\s*)(m_data_a)', r'\1if (ghost_data == NULL) continue;\n\t\t\t\2', content)

# Remove the debug prints just in case
content = re.sub(r'printf\("Rank.*?;\n', '', content)

with open('src/main.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Patch applied successfully.")
