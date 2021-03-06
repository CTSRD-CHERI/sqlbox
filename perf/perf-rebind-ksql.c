/*	$Id$ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "perf.h"
#include <ksql.h>

static	const char *const stmts[] = {
	"CREATE TABLE test (col1 INT, col2 INT, col3 INT, col4 INT)",
	"INSERT INTO test (col1,col2,col3,col4) VALUES (?,?,?,?)",
};

int
main(int argc, char *argv[])
{
	struct ksql	*sql;
	struct ksqlcfg	 cfg;
	struct ksqlstmt	*stmt;
	int		 c;
	int64_t		 v1, v2, v3, v4;
	size_t		 i, rows = 10000;

	if (pledge("stdio rpath cpath wpath flock fattr proc", NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	while ((c = getopt(argc, argv, "n:")) != -1)
		switch (c) {
		case 'n':
			rows = atoi(optarg);
			break;
		default:
			return EXIT_FAILURE;
		}

	memset(&cfg, 0, sizeof(struct ksqlcfg));

	cfg.flags = KSQL_EXIT_ON_ERR | KSQL_SAFE_EXIT;
	cfg.err = ksqlitemsg;
	cfg.dberr = ksqlitedbmsg;
	cfg.stmts.stmts = stmts;
	cfg.stmts.stmtsz = 2;

	printf(">>> %zu insertions\n", rows);
	fflush(stdout);

	if ((sql = ksql_alloc_child(&cfg, NULL, NULL)) == NULL)
		errx(EXIT_FAILURE, "ksql_alloc_child");

	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	if (ksql_open(sql, ":memory:") != KSQL_OK)
		errx(EXIT_FAILURE, "ksql_open");
	if (ksql_exec(sql, NULL, 0) != KSQL_OK)
		errx(EXIT_FAILURE, "ksql_exec");
	if (ksql_stmt_alloc(sql, &stmt, NULL, 1) != KSQL_OK)
		errx(EXIT_FAILURE, "ksql_stmt_alloc");

	for (i = 0; i < rows; i++) {
#ifdef __OpenBSD__
		v1 = arc4random();
		v2 = arc4random();
		v3 = arc4random();
		v4 = arc4random();
#else
		v1 = random();
		v2 = random();
		v3 = random();
		v4 = random();
#endif
		if (ksql_stmt_reset(stmt) != KSQL_OK)
			errx(EXIT_FAILURE, "ksql_stmt_reset");
		if (ksql_bind_int(stmt, 0, v1) != KSQL_OK)
			errx(EXIT_FAILURE, "ksql_bind_int");
		if (ksql_bind_int(stmt, 1, v2) != KSQL_OK)
			errx(EXIT_FAILURE, "ksql_bind_int");
		if (ksql_bind_int(stmt, 2, v3) != KSQL_OK)
			errx(EXIT_FAILURE, "ksql_bind_int");
		if (ksql_bind_int(stmt, 3, v4) != KSQL_OK)
			errx(EXIT_FAILURE, "ksql_bind_int");
		if (ksql_stmt_step(stmt) != KSQL_DONE)
			errx(EXIT_FAILURE, "ksql_stmt_step");
	}

	if (ksql_stmt_free(stmt) != KSQL_OK)
		errx(EXIT_FAILURE, "ksql_stmt_free");
	if (ksql_close(sql) != KSQL_OK)
		errx(EXIT_FAILURE, "ksql_close");

	ksql_free(sql);
	puts("<<< done");
	return EXIT_SUCCESS;
}
