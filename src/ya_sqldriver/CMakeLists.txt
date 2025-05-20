add_library(ya_sqldriver STATIC
  yasqldriver.h
  yasqldriver.cpp
  isqldriver.h
  mongodb/mongodbdriver.h
  mongodb/mongodbdriver.cpp
  mysql/mysqldriver.h
  mysql/mysqldriver.cpp
  postgresql/postgresqldriver.h
  postgresql/postgresqldriver.cpp
  sqlite/sqlitedriver.h
  sqlite/sqlitedriver.cpp
)

target_include_directories(ya_sqldriver PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(ya_sqldriver PUBLIC sqlite3 mysql::concpp)
