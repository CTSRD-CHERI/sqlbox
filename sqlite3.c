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
#include "config.h"

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif 

#include <assert.h>
#include <stdlib.h>

#include <sqlite3.h>

#include "sqlbox.h"
#include "extern.h"

/* 
 * Actually prepare a statement "pst".
 * In the usual way we sleep if SQLite gives us a busy, locked, or weird
 * protocol error.
 * All other errors are real errorrs.
 * Returns the statement or NULL on failure.
 */
sqlite3_stmt *
sqlbox_wrap_prepare(struct sqlbox *box, struct sqlbox_db *db,
	const struct sqlbox_pstmt *pst)
{
	sqlite3_stmt	*stmt;
	int		 c;
	size_t		 attempt = 0;

	assert(pst != NULL && pst->stmt != NULL);
	sqlbox_debug(&box->cfg, "%s: sqlite3_prepare_v2: %s",
		db->src->fname, pst->stmt);

again:
	stmt = NULL;
	c = sqlite3_prepare_v2(db->db, pst->stmt, -1, &stmt, NULL);

	switch (c) {
	case SQLITE_BUSY:
	case SQLITE_LOCKED:
	case SQLITE_PROTOCOL:
		if (stmt != NULL) {
			sqlbox_debug(&box->cfg, 
				"%s: sqlite3_finalize: %s",
				db->src->fname, pst->stmt);
			sqlite3_finalize(stmt);
		}
		sqlbox_sleep(attempt++);
		goto again;
	case SQLITE_OK:
		assert(stmt != NULL);
		return stmt;
	default:
		break;
	}

	sqlbox_warnx(&box->cfg, "%s: sqlite3_prepare_v2: %s", 
		db->src->fname, sqlite3_errmsg(db->db));
	sqlbox_warnx(&box->cfg, "%s: statement: %s", 
		db->src->fname, pst->stmt);

	if (stmt == NULL)
		return NULL;

	sqlbox_debug(&box->cfg, "%s: sqlite3_finalize: %s",
		db->src->fname, pst->stmt);
	sqlite3_finalize(stmt);
	return NULL;
}

enum sqlbox_code
sqlbox_wrap_step(struct sqlbox *box, struct sqlbox_db *db,
	const struct sqlbox_pstmt *pst, sqlite3_stmt *stmt,
	size_t *cols, int cstep)
{
	size_t	 attempt = 0;
	int	 ccount;

	*cols = 0;

	assert(pst != NULL && pst->stmt != NULL);
	sqlbox_debug(&box->cfg, "%s: sqlite3_step: %s",
		db->src->fname, pst->stmt);

again_step:
	switch (sqlite3_step(stmt)) {
	case SQLITE_BUSY:
		/*
		 * FIXME: according to sqlite3_step(3), this
		 * should return if we're in a transaction.
		 */
		sqlbox_sleep(attempt++);
		goto again_step;
	case SQLITE_LOCKED:
	case SQLITE_PROTOCOL:
		sqlbox_sleep(attempt++);
		goto again_step;
	case SQLITE_DONE:
		return SQLBOX_CODE_OK;
	case SQLITE_ROW:
		if ((ccount = sqlite3_column_count(stmt)) > 0) {
			*cols = (size_t)ccount;
			return SQLBOX_CODE_OK;
		}
		sqlbox_warnx(&box->cfg, "%s: sqlbox_wrap_step: "
			"row without columns", db->src->fname);
		sqlbox_warnx(&box->cfg, "%s: statement: %s", 
			db->src->fname, pst->stmt);
		return SQLBOX_CODE_ERROR;
	case SQLITE_CONSTRAINT:
		if (cstep)
			return SQLBOX_CODE_CONSTRAINT;
		/* FALLTHROUGH */
	default:
		break;
	}

	sqlbox_warnx(&box->cfg, "%s: step: %s", 
		db->src->fname, sqlite3_errmsg(db->db));
	return SQLBOX_CODE_ERROR;
}
