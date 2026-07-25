import codecs, re
with codecs.open('src/main.cpp', 'r', 'utf-8') as f:
    c = f.read()
c = re.sub(r'(char\s+filename\[BUFSIZ\] = "";)', r'\1\n\tp4est_data_t\t\t*p4est_data = (p4est_data_t *)p4est->user_pointer;', c)
with codecs.open('src/main.cpp', 'w', 'utf-8') as f:
    f.write(c)
