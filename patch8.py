import codecs

with codecs.open('src/main.cpp', 'r', 'utf-8') as f:
    content = f.read()

old_code8 = 'my_user_data_t *m_user_data = (my_user_data_t *)user_data;\n\tquad_data_t\t\t*ghost_data = (quad_data_t  *)m_user_data->quad_data;'
new_code8 = 'quad_data_t\t\t*ghost_data = (quad_data_t  *)user_data;'
content = content.replace(old_code8, new_code8)

with codecs.open('src/main.cpp', 'w', 'utf-8') as f:
    f.write(content)
