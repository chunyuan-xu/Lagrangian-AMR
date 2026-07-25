import codecs

with codecs.open('src/main.cpp', 'r', 'utf-8') as f:
    content = f.read()

# Fix 1: quadrant_corner_to_point_matrix_assemble_callback
old_code1 = 'quad_data_t\t\t*ghost_data = (quad_data_t  *)user_data;'
new_code1 = 'my_user_data_t *m_user_data = (my_user_data_t *)user_data;\n\tquad_data_t\t\t*ghost_data = (quad_data_t  *)m_user_data->quad_data;'
content = content.replace(old_code1, new_code1)

# Fix 2: MatrixAssemble free
old_code2 = 'quadrant_corner_to_point_matrix_assemble_callback);        /* 求解器的矩阵装配 */\r\n}'
new_code2 = 'quadrant_corner_to_point_matrix_assemble_callback);        /* 求解器的矩阵装配 */\n\tfree(m_user_data);\n}'
content = content.replace(old_code2, new_code2)
old_code2_alt = 'quadrant_corner_to_point_matrix_assemble_callback);        /* 求解器的矩阵装配 */\n}'
content = content.replace(old_code2_alt, new_code2)

# Fix 3: quadrant_corner_velocity_callback
old_code3 = 'quad_data_t\t\t\t\t\t*ghost_data = (quad_data_t *)user_data;'
new_code3 = 'my_user_data_t *m_user_data = (my_user_data_t *)user_data;\n\tquad_data_t\t\t\t\t\t*ghost_data = (quad_data_t *)m_user_data->quad_data;'
content = content.replace(old_code3, new_code3)

# Fix 4: Get_Corner_Velos free
old_code4 = 'quadrant_corner_velocity_callback);  \r\n}'
new_code4 = 'quadrant_corner_velocity_callback);\n\tfree(m_user_data);\n}'
content = content.replace(old_code4, new_code4)
old_code4_alt = 'quadrant_corner_velocity_callback);  \n}'
content = content.replace(old_code4_alt, new_code4)

with codecs.open('src/main.cpp', 'w', 'utf-8') as f:
    f.write(content)
