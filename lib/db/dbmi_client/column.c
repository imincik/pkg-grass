/*!
 * \file db/dbmi_client/column.c
 * 
 * \brief DBMI Library (client) - column info
 *
 * (C) 1999-2008 by the GRASS Development Team
 *
 * This program is free software under the GNU General Public
 * License (>=v2). Read the file COPYING that comes with GRASS
 * for details.
 *
 * \author Joel Jones (CERL/UIUC), Radim Blazek
 * \author Update by Glynn Clement <glynn gclements.plus.com>
 * and Martin Landa <landa.martin gmail.com>
 */

#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/dbmi.h>
#include <grass/glocale.h>

/*!
  \brief Get column sqltype

  See db_sqltype_name().

  \param driver DB driver
  \param tab table name
  \param col column name

  \return column sqltype
  \return -1 on error
*/
int db_column_sqltype(dbDriver * driver, const char *tab, const char *col)
{
    dbTable *table;
    dbString table_name;
    dbColumn *column;
    int ncol, cl, type;

    type = -1;

    db_init_string(&table_name);
    db_set_string(&table_name, tab);

    if (db_describe_table(driver, &table_name, &table) != DB_OK)
	return -1;

    db_free_string(&table_name);
    ncol = db_get_table_number_of_columns(table);
    for (cl = 0; cl < ncol; cl++) {
	column = db_get_table_column(table, cl);
	if (strcmp(db_get_column_name(column), col) == 0) {
	    type = db_get_column_sqltype(column);
	    break;
	}
    }

    db_free_table(table);

    return type;
}

/*!
  \brief Get column ctype

  See db_sqltype_to_Ctype().

  \param driver DB driver
  \param tab table name
  \param col column name

  \return column Ctype
  \return -1 on error
 */
int db_column_Ctype(dbDriver * driver, const char *tab, const char *col)
{
    int type;

    if ((type = db_column_sqltype(driver, tab, col)) >= 0) {
	type = db_sqltype_to_Ctype(type);
	return type;
    }

    return -1;
}

/*!
  \brief Get column structure by table and column name.

  Column is set to new dbColumn structure or NULL if column was not found
  
  \param Driver DB driver
  \param tname table name
  \param cname column name
  \param[out] Column column structure to store within

  \return DB_OK on success
  \return DB_FAILED on failure
*/
int db_get_column(dbDriver * Driver, const char *tname, const char *cname,
		  dbColumn ** Column)
{
    int i, ncols, ret;
    dbTable *Table;
    dbColumn *Col, *NCol;
    dbString tabname;

    db_init_string(&tabname);
    db_set_string(&tabname, tname);

    if (db_describe_table(Driver, &tabname, &Table) != DB_OK) {
      G_warning(_("Unable to describe table <%s>"), tname);
	return DB_FAILED;
    }

    *Column = NULL;
    ret = DB_FAILED;

    ncols = db_get_table_number_of_columns(Table);
    G_debug(3, "ncol = %d", ncols);

    for (i = 0; i < ncols; i++) {
	Col = db_get_table_column(Table, i);
	if (G_strcasecmp(db_get_column_name(Col), cname) == 0) {
	    NCol = (dbColumn *) malloc(sizeof(dbColumn));
	    db_init_column(NCol);
	    db_set_string(&(NCol->columnName), db_get_column_name(Col));
	    db_set_string(&(NCol->description),
			  db_get_column_description(Col));
	    NCol->sqlDataType = Col->sqlDataType;
	    NCol->hostDataType = Col->hostDataType;
	    db_copy_value(&(NCol->value), &(Col->value));
	    NCol->dataLen = Col->dataLen;
	    NCol->precision = Col->precision;
	    NCol->scale = Col->scale;
	    NCol->nullAllowed = Col->nullAllowed;
	    NCol->hasDefaultValue = Col->hasDefaultValue;
	    NCol->useDefaultValue = Col->useDefaultValue;
	    db_copy_value(&(NCol->defaultValue), &(Col->defaultValue));
	    NCol->select = Col->select;
	    NCol->update = Col->update;

	    *Column = NCol;
	    ret = DB_OK;
	    break;
	}
    }
    db_free_table(Table);

    return ret;
}
