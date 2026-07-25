import codecs

with codecs.open('src/main.cpp', 'r', 'utf-8') as f:
    content = f.read()

# Fix p4est_new_ext min_level
content = content.replace(
    'p4est_new_ext(mpicomm,\t\t\t\t // MPI通信器\n\t\tconn,\t\t\t\t\t // 连通性\n\t\t1,\t\t\t\t\t\t // 初始细分层级\n\t\t7,\t\t\t\t\t\t // 最小细分层级',
    'p4est_new_ext(mpicomm,\t\t\t\t // MPI通信器\n\t\tconn,\t\t\t\t\t // 连通性\n\t\t1,\t\t\t\t\t\t // 初始细分层级\n\t\tctx.minus_level,\t\t\t\t\t\t // 最小细分层级'
)

# Fix ghost_data allocation
content = content.replace('p4est_data_t\t\t*ghost_data;', 'quad_data_t\t\t*ghost_data;')
content = content.replace('ghost_data = P4EST_ALLOC(p4est_data_t, ghost->ghosts.elem_count);', 'ghost_data = P4EST_ALLOC(quad_data_t, ghost->ghosts.elem_count);')

# Fix Lagrangian_replace_quads (coarsening)
content = content.replace(
    'parent_data = (quad_data_t *)incoming[0]->p.user_data;',
    'parent_data = (quad_data_t *)incoming[0]->p.user_data;\n\t\tget_quadrant_boundary_from_p4est(p4est, incoming[0]);'
)

# Fix Lagrangian_replace_quads (refining)
content = content.replace(
    'child_data = (quad_data_t *)incoming[i]->p.user_data;',
    'child_data = (quad_data_t *)incoming[i]->p.user_data;\n\t\t\tget_quadrant_boundary_from_p4est(p4est, incoming[i]);'
)

# Fix write_solution (PVTU)
pvtu_patch = '''	sc_array_destroy(temperature_array);

	// Patch PVTU file to include TimeValue for Tecplot/Paraview
	if (p4est->mpirank == 0) {
		char pvtu_filename[1024];
		snprintf(pvtu_filename, sizeof(pvtu_filename), "%s.pvtu", filename);
		FILE *f = fopen(pvtu_filename, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			long fsize = ftell(f);
			fseek(f, 0, SEEK_SET);
			char *string = (char *)malloc(fsize + 1);
			fread(string, 1, fsize, f);
			fclose(f);
			string[fsize] = 0;
			
			char *insert_pos = strstr(string, "</VTKFile>");
			if (insert_pos) {
				*insert_pos = '\\0';
				f = fopen(pvtu_filename, "wb");
				if (f) {
					fprintf(f, "%s", string);
					fprintf(f, "  <FieldData>\\n    <DataArray type=\\"Float64\\" Name=\\"TimeValue\\" NumberOfTuples=\\"1\\" format=\\"ascii\\">\\n      %.16g\\n    </DataArray>\\n  </FieldData>\\n</VTKFile>\\n", p4est_data->current_time);
					fclose(f);
				}
			}
			free(string);
		}
	}
}'''
content = content.replace('sc_array_destroy(temperature_array);\n}', pvtu_patch)
content = content.replace('sc_array_destroy(temperature_array);\r\n}', pvtu_patch)

with codecs.open('src/main.cpp', 'w', 'utf-8') as f:
    f.write(content)
