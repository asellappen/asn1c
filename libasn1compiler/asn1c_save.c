#include "asn1c_internal.h"

static int asn1c_dump_streams(arg_t *arg);
static int asn1c_print_streams(arg_t *arg);
static int asn1c_save_streams(arg_t *arg);
static int asn1c_copy_over(arg_t *arg, char *path);

int
asn1c_save_compiled_output(arg_t *arg, const char *datadir) {

	(void)datadir;

	TQ_FOR(arg->mod, &(arg->asn->modules), mod_next) {
		TQ_FOR(arg->expr, &(arg->mod->members), next) {
			if(asn1_lang_map[arg->expr->meta_type]
				[arg->expr->expr_type].type_cb) {
				if(asn1c_dump_streams(arg))
					return -1;
			}
		}
	}

	/*
	 * Dump out the Makefile template and the rest of the support code.
	 */
	if((arg->flags & A1C_PRINT_COMPILED) == 0
	&& (arg->flags & A1C_OMIT_SUPPORT_CODE) == 0) {
		glob_t pg;
		FILE *mkf;
		char *p;
		int i;

		i = strlen(datadir) + sizeof("/*.[ch]");
		p = alloca(i);
		snprintf(p, i, "%s/*.[ch]", datadir);

		memset(&pg, 0, sizeof(pg));
		if(glob(p, GLOB_ERR
#ifdef	GLOB_TILDE
			| GLOB_TILDE
#endif	/* GLOB_TILDE */
		, NULL, &pg)) {
			fprintf(stderr,
				"Bad skeletons directory (-S) %s: %s\n",
				datadir, strerror(errno));
			return -1;
		}

		mkf = asn1c_open_file(arg, "Makefile.am", ".sample");
		if(mkf == NULL) {
			globfree(&pg);
			perror("Makefile.am.sample");
			return -1;
		}

		fprintf(mkf, "ASN_SRCS=");
		TQ_FOR(arg->mod, &(arg->asn->modules), mod_next) {
			TQ_FOR(arg->expr, &(arg->mod->members), next) {
				if(asn1_lang_map[arg->expr->meta_type]
					[arg->expr->expr_type].type_cb) {
					fprintf(mkf, "\t\\\n\t%s.c %s.h",
					arg->expr->Identifier,
					arg->expr->Identifier);
				}
			}
		}

		for(i = 0; i < pg.gl_pathc; i++) {
			if(asn1c_copy_over(arg, pg.gl_pathv[i])) {
				fprintf(mkf, ">>>ABORTED<<<");
				fclose(mkf);
				globfree(&pg);
				return -1;
			} else {
				fprintf(mkf, "\t\\\n\t%s",
					basename(pg.gl_pathv[i]));
			}
		}

		fprintf(mkf, "\n\n");
		fprintf(mkf, "lib_LTLIBRARIES=libsomething.la\n");
		fprintf(mkf, "libsomething_la_SOURCES=${ASN_SRCS}\n");
		fclose(mkf);
		fprintf(stderr, "Generated Makefile.am.sample\n");
		globfree(&pg);
	}

	return 0;
}

/*
 * Dump the streams.
 */
static int
asn1c_dump_streams(arg_t *arg)  {
	if(arg->flags & A1C_PRINT_COMPILED) {
		return asn1c_print_streams(arg);
	} else {
		return asn1c_save_streams(arg);
	}
}

static int
asn1c_print_streams(arg_t *arg)  {
	compiler_streams_t *cs = arg->expr->data;
	asn1p_expr_t *expr = arg->expr;
	int i;

	for(i = 0; i < OT_MAX; i++) {
		out_chunk_t *ot;
		if(TQ_FIRST(&cs->targets[i]) == NULL)
			continue;

		printf("\n/*** <<< %s [%s] >>> ***/\n\n",
			_compiler_stream2str[i],
			expr->Identifier);

		TQ_FOR(ot, &(cs->targets[i]), next) {
			fwrite(ot->buf, ot->len, 1, stdout);
		}
	}

	return 0;
}

static int
asn1c_save_streams(arg_t *arg)  {
	asn1p_expr_t *expr = arg->expr;
	compiler_streams_t *cs = expr->data;
	out_chunk_t *ot;
	FILE *fp_c, *fp_h;
	char *header_id;

	if(cs == NULL) {
		fprintf(stderr, "Cannot compile %s at line %d\n",
			expr->Identifier, expr->_lineno);
		return -1;
	}

	fp_c = asn1c_open_file(arg, expr->Identifier, ".c");
	fp_h = asn1c_open_file(arg, expr->Identifier, ".h");
	if(fp_c == NULL || fp_h == NULL) {
		if(fp_c) fclose(fp_c);	/* lacks unlink() */
		if(fp_h) fclose(fp_h);	/* lacks unlink() */
		return -1;
	}

	header_id = alloca(strlen(expr->Identifier) + 1);
	if(1) {
		char *src, *dst;
		for(src = expr->Identifier, dst = header_id;
				(*dst=*src); src++, dst++)
			if(!isalnum(*src)) *dst = '_';
		*dst = '\0';
	}

	fprintf(fp_h,
		"#ifndef\t_%s_H_\n"
		"#define\t_%s_H_\n"
		"\n", header_id, header_id);

	fprintf(fp_h, "#include <constr_TYPE.h>\n\n");

	TQ_FOR(ot, &(cs->targets[OT_DEPS]), next)
		fwrite(ot->buf, ot->len, 1, fp_h);
	fprintf(fp_h, "\n");
	TQ_FOR(ot, &(cs->targets[OT_TYPE_DECLS]), next)
		fwrite(ot->buf, ot->len, 1, fp_h);
	fprintf(fp_h, "\n");
	TQ_FOR(ot, &(cs->targets[OT_FUNC_DECLS]), next)
		fwrite(ot->buf, ot->len, 1, fp_h);

	fprintf(fp_c, "#include <%s.h>\n\n", expr->Identifier);
	TQ_FOR(ot, &(cs->targets[OT_STAT_DEFS]), next)
		fwrite(ot->buf, ot->len, 1, fp_c);
	TQ_FOR(ot, &(cs->targets[OT_CODE]), next)
		fwrite(ot->buf, ot->len, 1, fp_c);

	assert(OT_MAX == 5);

	fprintf(fp_h, "\n#endif\t/* _%s_H_ */\n", header_id);

	fclose(fp_c);
	fclose(fp_h);
	fprintf(stderr, "Compiled %s.c\n", expr->Identifier);
	fprintf(stderr, "Compiled %s.h\n", expr->Identifier);
	return 0;
}

static int
asn1c_copy_over(arg_t *arg, char *path) {
	char *fname = basename(path);

	if(symlink(path, fname)) {
		if(errno == EEXIST) {
			struct stat sb1, sb2;
			if(stat(path, &sb1) == 0
			&& stat(fname, &sb2) == 0
			&& sb1.st_dev == sb2.st_dev
			&& sb1.st_ino == sb2.st_ino) {
				/*
				 * Nothing to do.
				 */
				fprintf(stderr,
					"File %s is already here as %s\n",
					path, fname);
				return 0;
			} else {
				fprintf(stderr,
					"Retaining local %s (%s suggested)\n",
					fname, path);
				return 0;
			}
		} else {
			fprintf(stderr, "Symlink %s -> %s failed: %s\n",
				path, fname, strerror(errno));
			return -1;
		}
	}

	fprintf(stderr, "Symlinked %s\t-> %s\n", path, fname);

	return 0;
}

