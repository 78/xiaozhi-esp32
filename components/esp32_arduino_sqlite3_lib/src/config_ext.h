#define BUILD_sqlite -DNDEBUG
#define SQLITE_OMIT_LOAD_EXTENSION           1
#define SQLITE_DQS                           0
#define SQLITE_OS_OTHER                      1
#define SQLITE_NO_SYNC                       1
#define SQLITE_TEMP_STORE                    1
#define SQLITE_DISABLE_LFS                   1
#define SQLITE_DISABLE_DIRSYNC               1
#define SQLITE_SECURE_DELETE                 0
#define SQLITE_DEFAULT_LOOKASIDE        512,64
#define YYSTACKDEPTH                        20
#define SQLITE_SMALL_STACK                   1
#define SQLITE_SORTER_PMASZ                  4
#define SQLITE_DEFAULT_CACHE_SIZE           -1
#define SQLITE_DEFAULT_MEMSTATUS             0
#define SQLITE_DEFAULT_MMAP_SIZE             0
#define SQLITE_CORE                          1
#define SQLITE_SYSTEM_MALLOC                 1
#define SQLITE_THREADSAFE                    0
#define SQLITE_MUTEX_APPDEF                  1
#define SQLITE_OMIT_WAL                      1
#define SQLITE_DISABLE_FTS3_UNICODE          1
#define SQLITE_DISABLE_FTS4_DEFERRED         1
#define SQLITE_LIKE_DOESNT_MATCH_BLOBS       1
#define SQLITE_DEFAULT_FOREIGN_KEYS          1
#define SQLITE_DEFAULT_LOCKING_MODE          1
#define SQLITE_DEFAULT_PAGE_SIZE           512
#define SQLITE_DEFAULT_PCACHE_INITSZ         8
#define SQLITE_MAX_DEFAULT_PAGE_SIZE     32768
#define SQLITE_POWERSAFE_OVERWRITE           1
#define SQLITE_MAX_EXPR_DEPTH                0
#undef SQLITE_OMIT_FOREIGN_KEY

/*
#undef SQLITE_OMIT_ALTERTABLE
#undef SQLITE_OMIT_ANALYZE
#undef SQLITE_OMIT_ATTACH
#define SQLITE_OMIT_AUTHORIZATION            1
#undef SQLITE_OMIT_AUTOINCREMENT
#define SQLITE_OMIT_AUTOINIT                 1
#define SQLITE_OMIT_AUTOMATIC_INDEX          1
#define SQLITE_OMIT_AUTORESET                1
#define SQLITE_OMIT_AUTOVACUUM               1
#undef SQLITE_OMIT_BETWEEN_OPTIMIZATION
#define SQLITE_OMIT_BLOB_LITERAL             1
#define SQLITE_OMIT_BTREECOUNT               1
#define SQLITE_OMIT_BUILTIN_TEST             1
#define SQLITE_OMIT_CAST                     1
#define SQLITE_OMIT_CHECK                    1
#define SQLITE_OMIT_COMPILEOPTION_DIAGS      1
#define SQLITE_OMIT_COMPOUND_SELECT          1
#define SQLITE_OMIT_CONFLICT_CLAUSE          1
#undef SQLITE_OMIT_CTE
#define SQLITE_OMIT_DECLTYPE                 1
#define SQLITE_OMIT_DEPRECATED               1
#undef SQLITE_OMIT_DISKIO
#define SQLITE_OMIT_EXPLAIN                  1
#define SQLITE_OMIT_FLAG_PRAGMAS             1
#define SQLITE_OMIT_FOREIGN_KEY              1
#define SQLITE_OMIT_GET_TABLE                1
#define SQLITE_OMIT_INCRBLOB                 1
#define SQLITE_OMIT_INTEGRITY_CHECK          1
#undef SQLITE_OMIT_LIKE_OPTIMIZATION
#undef SQLITE_OMIT_LOCALTIME
#define SQLITE_OMIT_LOOKASIDE                1
#undef SQLITE_OMIT_MEMORYDB
#undef SQLITE_OMIT_OR_OPTIMIZATION
#undef SQLITE_OMIT_PAGER_PRAGMAS
#define SQLITE_OMIT_PARSER_TRACE             1
#undef SQLITE_OMIT_PRAGMA
#define SQLITE_OMIT_PROGRESS_CALLBACK        1
#define SQLITE_OMIT_QUICKBALANCE             1
#undef SQLITE_OMIT_REINDEX
#define SQLITE_OMIT_SCHEMA_PRAGMAS           1
#define SQLITE_OMIT_SCHEMA_VERSION_PRAGMAS   1
#define SQLITE_OMIT_SHARED_CACHE             1
#define SQLITE_OMIT_TCL_VARIABLE             1
#define SQLITE_OMIT_TEMPDB                   1
#define SQLITE_OMIT_TRACE                    1
#undef SQLITE_OMIT_TRIGGER
#define SQLITE_OMIT_TRUNCATE_OPTIMIZATION    1
#define SQLITE_OMIT_UTF16                    1
#undef SQLITE_OMIT_VACUUM
#undef SQLITE_OMIT_VIEW
#undef SQLITE_OMIT_VIRTUALTABLE
#undef SQLITE_OMIT_WSD
#define SQLITE_OMIT_XFER_OPT                 1
#define SQLITE_PERFORMANCE_TRACE             1
//#define SQLITE_OMIT_COMPLETE              1
//#define SQLITE_OMIT_SUBQUERY              1
//#define SQLITE_OMIT_DATETIME_FUNCS        1
//#define SQLITE_OMIT_FLOATING_POINT        1
#define SQLITE_COUNTOFVIEW_OPTIMIZATION      0
*/
