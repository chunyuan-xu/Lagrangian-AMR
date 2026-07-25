import codecs

with codecs.open('src/main.cpp', 'r', 'utf-8') as f:
    content = f.read()

# Fix p4est_ghost_exchange_data
old_code5 = 'get_boundary_from_p4est(p4est);'
new_code5 = 'get_boundary_from_p4est(p4est);\n\tp4est_ghost_exchange_data(p4est, ghost, ghost_data);'
content = content.replace(old_code5, new_code5)

with codecs.open('src/main.cpp', 'w', 'utf-8') as f:
    f.write(content)
