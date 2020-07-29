/**
 * Sass
 * PHP bindings to libsass - fast, native Sass parsing in PHP!
 *
 * https://github.com/jamierumbelow/sassphp
 * Copyright (c)2012 Jamie Rumbelow <http://jamierumbelow.net>
 *
 * Fork updated and maintained by https://github.com/absalomedia
 */

#include <stdio.h>
#include <stdlib.h>

#include "php_sass.h"
#include "utilities.h"

/* --------------------------------------------------------------
 * Sass
 * ------------------------------------------------------------ */

zend_object_handlers sass_handlers;

typedef struct sass_object {
    int style;
    char* include_paths;
    bool comments;
    bool indent;
    long precision;
    char* map_path;
    bool omit_map_url;
    bool map_embed;
    bool map_contents;
    char* map_root;
    zval importer;
    zval function_table;
    zend_object zo;
} sass_object;

zend_class_entry *sass_ce;

static void sass_value_dtor(zend_resource *rsrc)
{
    sass_delete_value((union Sass_Value *)rsrc->ptr);
}

static inline sass_object *sass_fetch_object(zend_object *obj)
{
    return (sass_object *) ((char*) (obj) - XtOffsetOf(sass_object, zo));
}

#define Z_SASS_P(zv) sass_fetch_object(Z_OBJ_P((zv)));

static void sass_free_storage(zend_object *object)
{
    sass_object *obj = sass_fetch_object(object);
    if (obj->include_paths != NULL)
        efree(obj->include_paths);

    if (obj->map_path != NULL)
        efree(obj->map_path);
    if (obj->map_root != NULL)
        efree(obj->map_root);

    zval_ptr_dtor(&obj->importer);
    zval_ptr_dtor(&obj->function_table);

    zend_object_std_dtor(object);

}

zend_object * sass_create_handler(zend_class_entry *type) {
    size_t size = sizeof(sass_object) + zend_object_properties_size(type);

    sass_object *obj = emalloc(size);
    memset(obj, 0, size - sizeof(zval));

    zend_object_std_init(&obj->zo, type);
    object_properties_init(&obj->zo, type);
    obj->zo.handlers = &sass_handlers;

    return &obj->zo;
}

char *to_c_string(zval *var){
    if (Z_TYPE_P(var) != IS_STRING) {
        convert_to_string(var);
    }
    return Z_STRVAL_P(var);
}

Sass_Import_Entry array_to_import(zval* val){
    if (Z_TYPE_P(val) != IS_ARRAY){
        return NULL;
    }

    int len = zend_hash_num_elements(Z_ARRVAL_P(val));
    if (len < 1){
        zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Need at least redirected path");
        return NULL;
    }

    char *file = 0;
    zval *temp = zend_hash_index_find(Z_ARRVAL_P(val), 0);
    if (temp != NULL && !Z_ISUNDEF_P(temp) && Z_TYPE_P(temp) != IS_NULL){
        file = sass_copy_c_string(to_c_string(temp));
    }

    char *content = 0;
    temp = zend_hash_index_find(Z_ARRVAL_P(val), 1);
    if (temp != NULL && !Z_ISUNDEF_P(temp) && Z_TYPE_P(temp) != IS_NULL){
        content = sass_copy_c_string(to_c_string(temp));
    }

    char *map = 0;
    if (len >= 3){
        temp = zend_hash_index_find(Z_ARRVAL_P(val), 2);
        if (temp != NULL && !Z_ISUNDEF_P(temp) && Z_TYPE_P(temp) != IS_NULL){
            map = sass_copy_c_string(to_c_string(temp));
        }
    }

    return sass_make_import_entry(
        file, content, map
    );
}

Sass_Import_List sass_importer(const char* path, Sass_Importer_Entry cb, struct Sass_Compiler* comp){

    sass_object *obj = (sass_object *) sass_importer_get_cookie(cb);
    if (obj == NULL){
        zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Internal Error: Failed to retrieve object reference");
        return NULL;
    }

    zval cb_args[1];
    zval cb_retval;
    ZVAL_STRING(&cb_args[0], path);

    if (call_user_function_ex(EG(function_table), NULL, &obj->importer, &cb_retval, 1, cb_args, 0, NULL) != SUCCESS || Z_ISUNDEF(cb_retval)) {
        zval_ptr_dtor(&cb_args[0]);
        return NULL;
    }
    zval_ptr_dtor(&cb_args[0]);

    if (Z_TYPE(cb_retval) == IS_NULL){
        zval_ptr_dtor(&cb_retval);
        return NULL;
    }

    if (Z_TYPE(cb_retval) != IS_ARRAY){
        zval_ptr_dtor(&cb_retval);
        zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Importer callback must return an array");
        return NULL;
    }

    int result_len = zend_hash_num_elements(Z_ARRVAL(cb_retval));
    if (result_len < 1){
        zval_ptr_dtor(&cb_retval);
        return NULL;
    }


    zval *first_element = zend_hash_index_find(Z_ARRVAL(cb_retval), 0);
    if (first_element == NULL){
        zval_ptr_dtor(&cb_retval);
        zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Importer callback must return an array");
        return NULL;
    }

    Sass_Import_List list;
    if (Z_TYPE_P(first_element) == IS_ARRAY){
        list = sass_make_import_list(result_len);
        int idx = 0;
        zval *element;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL(cb_retval), element) {
            if (Z_TYPE_P(element) != IS_ARRAY){
                zval_ptr_dtor(&cb_retval);
                zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Importer callback must return an array");
                return NULL;
            }
            Sass_Import_Entry imp = array_to_import(element);
            if (imp == NULL) return NULL;
            list[idx] = imp;
            idx++;
        } ZEND_HASH_FOREACH_END();
    }else{
        list = sass_make_import_list(1);
        list[0] = array_to_import(&cb_retval);
    }

    zval_ptr_dtor(&cb_retval);
    return list;
}

void sass_convert_to_zval(const sass_object* const obj, const union Sass_Value* const s_args, zval* const out) {
    switch (sass_value_get_tag(s_args)) {
        case SASS_BOOLEAN: {
            ZVAL_BOOL(out, sass_boolean_get_value(s_args));
        } break;
        case SASS_NUMBER: {
            if (strlen(sass_number_get_unit(s_args)) == 0) {
                ZVAL_DOUBLE(out, sass_number_get_value(s_args));
            }
            else {
                union Sass_Value* const string = sass_value_stringify(s_args, false, obj->precision);
                ZVAL_STRING(out, sass_string_get_value(string));
                sass_delete_value(string);
            }
        } break;
        case SASS_COLOR: {
            char tmp[] = "#00000000";
            unsigned char r, g, b, a;

            r = sass_color_get_r(s_args);
            g = sass_color_get_g(s_args);
            b = sass_color_get_b(s_args);
            a = fmin(sass_color_get_a(s_args) * 255, 255);

            if (a == 255) {
                snprintf(tmp, sizeof(tmp), "#%02x%02x%02x", r, g, b);
            }
            else {
                snprintf(tmp, sizeof(tmp), "#%02x%02x%02x%02x", r, g, b, a);
            }

            ZVAL_STRING(out, tmp);
        } break;
        case SASS_STRING: {
            ZVAL_STRING(out, sass_string_get_value(s_args));
        } break;
        case SASS_LIST: {
            array_init(out);

            for (size_t len = sass_list_get_length(s_args), i = 0; i < len; ++i) {
                zval tmp;

                sass_convert_to_zval(obj, sass_list_get_value(s_args, i), &tmp);
                add_index_zval(out, i, &tmp);
            }
        } break;
        case SASS_MAP: {
            array_init(out);

            for (size_t len = sass_map_get_length(s_args), i = 0; i < len; ++i) {
                const union Sass_Value* map_key = sass_map_get_key(s_args, i);
                const enum Sass_Tag map_key_tag = sass_value_get_tag(map_key);
                zval tmp;

                sass_convert_to_zval(obj, sass_map_get_value(s_args, i), &tmp);
                if (map_key_tag == SASS_NUMBER && strlen(sass_number_get_unit(map_key)) == 0) {
                    add_index_zval(out, sass_number_get_value(map_key), &tmp);
                }
                else if (map_key_tag == SASS_STRING) {
                    add_assoc_zval(out, sass_string_get_value(map_key), &tmp);
                }
                else {
                    union Sass_Value* const string = sass_value_stringify(map_key, false, obj->precision);
                    add_assoc_zval(out, sass_string_get_value(string), &tmp);
                    sass_delete_value(string);
                }
            }
        } break;
        default: {
            ZVAL_NULL(out);
        } break;
    }
}

union Sass_Value* sass_function(const union Sass_Value* s_args, Sass_Function_Entry cb, struct Sass_Compiler* comp)
{
    sass_object *obj = (sass_object *) sass_function_get_cookie(cb);
    if (obj == NULL){
        zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Internal Error: Failed to retrieve object reference");
        return NULL;
    }

    const char *signature = sass_function_get_signature(cb);

    if (Z_TYPE(obj->function_table) != IS_ARRAY){
        zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Internal Error: Function table has vanished");
        return NULL;
    }

    zend_string *fname = zend_string_init(signature, strlen(signature), 0);
    zval* callback = zend_hash_find(Z_ARRVAL(obj->function_table), fname);
    zend_string_release(fname);
    if (callback == NULL){
        return sass_make_null();
    }

    if (!zend_is_callable(callback, 0, NULL)) {
        zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Internal Error: value for sig %s lost its callbackyness", ZSTR_VAL(fname));
        return sass_make_null();
    }

    Sass_Import_Entry import = sass_compiler_get_last_import(comp);

    zval path_info;
    array_init(&path_info);
    add_assoc_string(&path_info, "absolute", (char*) sass_import_get_abs_path(import));
    add_assoc_string(&path_info, "relative", (char*) sass_import_get_imp_path(import));

    zval cb_args[2];
    zval cb_retval;
    sass_convert_to_zval(obj, s_args, &cb_args[0]);
    cb_args[1] = path_info;

    if (call_user_function_ex(EG(function_table), NULL, callback, &cb_retval, 2, cb_args, 0, NULL) != SUCCESS || Z_ISUNDEF(cb_retval)) {
        zval_ptr_dtor(&cb_args[0]);
        return sass_make_null();
    }
    zval_ptr_dtor(&cb_args[0]);
    zval_ptr_dtor(&cb_args[1]);

    if (Z_TYPE_P(&cb_retval) == IS_RESOURCE) {
        return sass_clone_value((union Sass_Value*) zend_fetch_resource(Z_RES_P(&cb_retval), "Sass_Value", sass_value_resnum));
    }
    else {
        zend_throw_exception_ex(sass_function_exception_ce, 0 TSRMLS_CC, "Function return value must be a resource of type 'Sass_Value'");
        return sass_make_null();
    }
}



PHP_METHOD(Sass, __construct)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_NULL();
    }
 
    sass_object *obj = Z_SASS_P(this);
 
    obj->style = SASS_STYLE_NESTED;
    obj->include_paths = NULL;
    obj->map_path = NULL;
    obj->map_root = NULL;
    obj->comments = false;
    obj->indent = false;
    obj->map_embed = false;
    obj->map_contents = false;
    obj->omit_map_url = true;
    obj->precision = 5;
    ZVAL_UNDEF(&obj->importer);
    ZVAL_UNDEF(&obj->function_table);
}


void set_options(sass_object *this, struct Sass_Context *ctx)
{
    struct Sass_Options* opts = sass_context_get_options(ctx);

    sass_option_set_precision(opts, this->precision);
    sass_option_set_output_style(opts, this->style);
    sass_option_set_is_indented_syntax_src(opts, this->indent);
    if (this->include_paths != NULL) {
    sass_option_set_include_path(opts, this->include_paths);
    }
    sass_option_set_source_comments(opts, this->comments);
    if (this->comments) {
    sass_option_set_omit_source_map_url(opts, false);
    }
    sass_option_set_source_map_embed(opts, this->map_embed);
    sass_option_set_source_map_contents(opts, this->map_contents);
    if (this->map_path != NULL) {
    sass_option_set_source_map_file(opts, this->map_path);
    sass_option_set_omit_source_map_url(opts, true);
    sass_option_set_source_map_contents(opts, false);
    }
    if (this->map_root != NULL) {
    sass_option_set_source_map_root(opts, this->map_root);
    }

    if (!Z_ISUNDEF(this->importer)) {
        Sass_Importer_Entry imp = sass_make_importer(sass_importer, 0, this);
        Sass_Importer_List imp_list = sass_make_importer_list(1);
        sass_importer_set_list_entry(imp_list, 0, imp);
        sass_option_set_c_importers(opts, imp_list);
    }

    if (!Z_ISUNDEF(this->function_table)) {
        int function_count = zend_hash_num_elements(Z_ARRVAL(this->function_table));

        Sass_Function_List fn_list = sass_make_function_list(function_count);
        int idx = 0;

        zend_ulong num_key;
        zend_string *string_key;
        zval *val;
        Sass_Function_Entry fn;

        ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL(this->function_table), num_key, string_key, val) {
            if (string_key == NULL){
                zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Keys must be function declarations");
                return;
            }
            if (!zend_is_callable(val, 0, NULL)) {
                zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Values must be callables, but value at `%s` isn't", ZSTR_VAL(string_key));
                return;
            }
            fn = sass_make_function(ZSTR_VAL(string_key), sass_function, (void*)this);
            sass_function_set_list_entry(fn_list, idx, fn);
            idx++;
        } ZEND_HASH_FOREACH_END();

        sass_option_set_c_functions(opts, fn_list);
    }


}

/**
 * $sass->parse(string $source, [  ]);
 *
 * Parse a string of Sass; a basic input -> output affair.
 */
PHP_METHOD(Sass, compile)
{
    sass_object *this = sass_fetch_object(Z_OBJ_P(getThis()));

    // Define our parameters as local variables
    char *source, *input_path = NULL;
    size_t source_len, input_path_len = 0;

    // Use zend_parse_parameters() to grab our source from the function call
    if (zend_parse_parameters_throw(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &source, &source_len, &input_path, &input_path_len) == FAILURE){
        RETURN_FALSE;
    }

    // Create a new sass_context
    struct Sass_Data_Context* data_context = sass_make_data_context(strdup(source));
    struct Sass_Context* ctx = sass_data_context_get_context(data_context);

    set_options(this, ctx);
    if (input_path != NULL){
        struct Sass_Options* opts = sass_context_get_options(ctx);
        sass_option_set_input_path(opts, input_path);
    }

    int status = sass_compile_data_context(data_context);

    // Check the context for any errors...
    if (status != 0)
    {
        zend_throw_exception(sass_exception_ce, sass_context_get_error_message(ctx), 0 TSRMLS_CC);
    }
    else
    {
        RETVAL_STRING(sass_context_get_output_string(ctx));
    }

    sass_delete_data_context(data_context);
}

/**
 * $sass->parse_file(string $file_name);
 *
 * Parse a whole file FULL of Sass and return the CSS output
 */
PHP_METHOD(Sass, compileFile)
{
    array_init(return_value);

    sass_object *this = Z_SASS_P(getThis());
 
    // We need a file name and a length
    char *file;
    size_t file_len;
 
    // Grab the file name from the function
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &file, &file_len) == FAILURE)
    {
        RETURN_FALSE;
    }

    // First, do a little checking of our own. Does the file exist?
    if( access( file, F_OK ) == -1 )
    {
        zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "File %s could not be found", file);
        RETURN_FALSE;
    }

    struct Sass_File_Context* file_ctx = sass_make_file_context(file);

    struct Sass_Context* ctx = sass_file_context_get_context(file_ctx);

    set_options(this, ctx);

    int status = sass_compile_file_context(file_ctx);

    // Check the context for any errors...
    if (status != 0)
    {
        zend_throw_exception(sass_exception_ce, sass_context_get_error_message(ctx), 0 TSRMLS_CC);
    }
    else
    {

        if (this->map_path != NULL ) {
        // Send it over to PHP.
        add_next_index_string(return_value, sass_context_get_output_string(ctx));
        } else {
        RETVAL_STRING(sass_context_get_output_string(ctx));
        }

         // Do we have source maps to go?
         if (this->map_path != NULL)
         {
         // Send it over to PHP.
         add_next_index_string(return_value, sass_context_get_source_map_string(ctx));
         }
    }

    sass_delete_file_context(file_ctx);
}

PHP_METHOD(Sass, getStyle)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);

    RETURN_LONG(obj->style);
}

PHP_METHOD(Sass, setStyle)
{
    zval *this = getThis();

    long new_style;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &new_style) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);
    obj->style = new_style;

    RETURN_NULL();
}

PHP_METHOD(Sass, getIncludePath)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);

    if (obj->include_paths == NULL) RETURN_STRING("");
    RETURN_STRING(obj->include_paths);
}

PHP_METHOD(Sass, setIncludePath)
{
    zval *this = getThis();

    char *path;
    size_t path_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE)
        RETURN_FALSE;

    sass_object *obj = Z_SASS_P(this);

    if (obj->include_paths != NULL)
        efree(obj->include_paths);
    obj->include_paths = estrndup(path, path_len);

    RETURN_NULL();
}

PHP_METHOD(Sass, getMapPath)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);
    if (obj->map_path == NULL) RETURN_STRING("");
    RETURN_STRING(obj->map_path);
}

PHP_METHOD(Sass, setMapPath)
{
    zval *this = getThis();

    char *path;
    size_t path_len;
 
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE)
        RETURN_FALSE;

    sass_object *obj = Z_SASS_P(this);
    if (obj->map_path != NULL)
        efree(obj->map_path);
    obj->map_path = estrndup(path, path_len);

    RETURN_NULL();
}

PHP_METHOD(Sass, getMapRoot)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);
    if (obj->map_root == NULL) RETURN_STRING("");
    RETURN_STRING(obj->map_root);
}

PHP_METHOD(Sass, setMapRoot)
{
    zval *this = getThis();

    char *path;
    size_t path_len;
 
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE)
        RETURN_FALSE;

    sass_object *obj = Z_SASS_P(this);
    if (obj->map_root != NULL)
        efree(obj->map_root);
    obj->map_root = estrndup(path, path_len);

    RETURN_NULL();
}


PHP_METHOD(Sass, getPrecision)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);
    RETURN_LONG(obj->precision);
}

PHP_METHOD(Sass, setPrecision)
{
    zval *this = getThis();

    long new_precision;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &new_precision) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);
    obj->precision = new_precision;

    RETURN_NULL();
}

PHP_METHOD(Sass, getEmbed)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);
    
    RETURN_LONG(obj->map_embed);
}

PHP_METHOD(Sass, setEmbed)
{
    zval *this = getThis();

    bool new_map_embed;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &new_map_embed) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);
    
    obj->map_embed = new_map_embed;

    RETURN_NULL();
}

PHP_METHOD(Sass, getComments)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);

    RETURN_LONG(obj->comments);
}

PHP_METHOD(Sass, setComments)
{
    zval *this = getThis();

    bool new_comments;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &new_comments) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);

    obj->comments = new_comments;

    RETURN_NULL();
}

PHP_METHOD(Sass, getIndent)
{
    zval *this = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);

    RETURN_LONG(obj->indent);
}

PHP_METHOD(Sass, setIndent)
{
    zval *this = getThis();

    bool new_indent;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &new_indent) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(this);

    obj->indent = new_indent;

    RETURN_NULL();
}

PHP_METHOD(Sass, setImporter)
{
    zval *importer;
    zend_string *callback_name;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &importer) == FAILURE) {
        RETURN_FALSE;
    }

    sass_object *obj = Z_SASS_P(getThis());

    if (Z_TYPE_P(importer) == IS_NULL){
        if (!Z_ISUNDEF(obj->importer)) {
            zval_ptr_dtor(&obj->importer);
        }
        ZVAL_UNDEF(&obj->importer);
        RETURN_TRUE;
    }

    if (!zend_is_callable(importer, 0, &callback_name)) {
        php_error_docref(NULL, E_WARNING, "%s is not a valid callable", ZSTR_VAL(callback_name));
        zend_string_release(callback_name);
        RETURN_FALSE;
    }

    if (!Z_ISUNDEF(obj->importer)) {
        zval_ptr_dtor(&obj->importer);
        ZVAL_UNDEF(&obj->importer);
    }

    ZVAL_COPY(&obj->importer, importer);
    RETURN_TRUE;
}

PHP_METHOD(Sass, setFunctions)
{
    zval *funcs;

    if (zend_parse_parameters_throw(ZEND_NUM_ARGS(), "a!", &funcs) == FAILURE) {
        return;
    }

    sass_object *obj = Z_SASS_P(getThis());

    if (funcs == NULL){
        if (!Z_ISUNDEF(obj->function_table)) {
            zval_ptr_dtor(&obj->function_table);
        }
        ZVAL_UNDEF(&obj->function_table);
        RETURN_TRUE;
    }

    zend_ulong num_key;
    zend_string *string_key;
    zval *val;

    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(funcs), num_key, string_key, val) {
        if (string_key == NULL){
            zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Keys must be function declarations");
        }
        if (!zend_is_callable(val, 0, NULL)) {
            zend_throw_exception_ex(sass_exception_ce, 0 TSRMLS_CC, "Values must be callables, but value at `%s` isn't", ZSTR_VAL(string_key));
            RETURN_FALSE;
        }
    } ZEND_HASH_FOREACH_END();

    if (!Z_ISUNDEF(obj->function_table)) {
        zval_ptr_dtor(&obj->function_table);
        ZVAL_UNDEF(&obj->function_table);
    }

    ZVAL_COPY(&obj->function_table, funcs);
    RETURN_TRUE;
}

PHP_METHOD(Sass, getLibraryVersion)
{
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    RETURN_STRING(libsass_version());

}

PHP_FUNCTION(sass_make_null)
{
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "", NULL) == FAILURE) {
        RETURN_FALSE;
    }

    ZVAL_RES(return_value, zend_register_resource((void*)sass_make_null(), sass_value_resnum));
}

PHP_FUNCTION(sass_make_boolean)
{
    bool val;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &val) == FAILURE) {
        RETURN_FALSE;
    }

    ZVAL_RES(return_value, zend_register_resource((void*)sass_make_boolean(val), sass_value_resnum));
}

PHP_FUNCTION(sass_make_string)
{
    char *str;
    size_t str_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
        RETURN_FALSE;
    }

    ZVAL_RES(return_value, zend_register_resource((void*)sass_make_string(str), sass_value_resnum));
}

PHP_FUNCTION(sass_make_qstring)
{
    char *str;
    size_t str_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
        RETURN_FALSE;
    }

    ZVAL_RES(return_value, zend_register_resource((void*)sass_make_qstring(str), sass_value_resnum));
}

PHP_FUNCTION(sass_make_number)
{
    double number;
    char *unit = NULL;
    size_t unit_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d|s", &number, &unit, &unit_len) == FAILURE) {
        RETURN_FALSE;
    }

    ZVAL_RES(return_value, zend_register_resource((void*)sass_make_number(number, unit ?: ""), sass_value_resnum));
}

PHP_FUNCTION(sass_make_color)
{
    double r, g, b, a = 1;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ddd|d", &r, &g, &b, &a) == FAILURE) {
        RETURN_FALSE;
    }

    ZVAL_RES(return_value, zend_register_resource((void*)sass_make_color(r, g, b, a), sass_value_resnum));
}

PHP_FUNCTION(sass_make_list)
{
    zval *arr = NULL;
    char *sep = " ";
    size_t sep_len;
    bool bracketed = false;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|asb", &arr, &sep, &sep_len, &bracketed) == FAILURE) {
        RETURN_FALSE;
    }

    enum Sass_Separator sass_sep = SASS_SPACE;
    if (sep_len > 0 && strncmp(sep, ",", fmin(sep_len, 1)) == 0) {
        sass_sep = SASS_COMMA;
    }
    else if (strncmp(sep, " ", fmin(sep_len, 1)) != 0) {
        zend_throw_exception_ex(sass_value_exception_ce, 0 TSRMLS_CC, "Separator must be a space (' ') or a comma (',')");
        return;
    }

    if (!arr) {
        ZVAL_RES(return_value, zend_register_resource((void*)sass_make_list(0, sass_sep, bracketed), sass_value_resnum));
        return;
    }

    union Sass_Value *list = sass_make_list(zend_hash_num_elements(Z_ARRVAL_P(arr)), sass_sep, bracketed);

    zend_ulong idx = 0;
    zval *element;

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(arr), element) {
        if (Z_TYPE_P(element) == IS_RESOURCE) {
            sass_list_set_value(list, idx, sass_clone_value((union Sass_Value*) zend_fetch_resource(Z_RES_P(element), "Sass_Value", sass_value_resnum)));
        }
        else {
            zend_throw_exception_ex(sass_value_exception_ce, 0 TSRMLS_CC, "List values must be a resource of type 'Sass_Value'");
            sass_delete_value(list);
            return;
        }

        ++idx;
    } ZEND_HASH_FOREACH_END();

    ZVAL_RES(return_value, zend_register_resource((void*)list, sass_value_resnum));
}

PHP_FUNCTION(sass_make_map)
{
    zval *arr = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a", &arr) == FAILURE) {
        RETURN_FALSE;
    }

    if (!arr) {
        ZVAL_RES(return_value, zend_register_resource((void*)sass_make_map(0), sass_value_resnum));
        return;
    }

    union Sass_Value *map = sass_make_map(zend_hash_num_elements(Z_ARRVAL_P(arr)));

    zval *element;
    zend_ulong idx, num_idx;
    zend_string *str_idx;

    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(arr), num_idx, str_idx, element) {
        if (Z_TYPE_P(element) == IS_RESOURCE) {
            if (str_idx) {
                sass_map_set_key(map, idx, sass_make_string(ZSTR_VAL(str_idx)));
            }
            else {
                sass_map_set_key(map, idx, sass_make_number(num_idx, ""));
            }

            sass_map_set_value(map, idx, sass_clone_value((union Sass_Value*) zend_fetch_resource(Z_RES_P(element), "Sass_Value", sass_value_resnum)));
        }
        else {
            zend_throw_exception_ex(sass_value_exception_ce, 0 TSRMLS_CC, "List values must be a resource of type 'Sass_Value'");
            sass_delete_value(map);
            return;
        }

        ++idx;
    } ZEND_HASH_FOREACH_END();

    ZVAL_RES(return_value, zend_register_resource((void*)map, sass_value_resnum));
}

PHP_FUNCTION(sass_make_error)
{
    char *str;
    size_t str_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
        RETURN_FALSE;
    }

    ZVAL_RES(return_value, zend_register_resource((void*)sass_make_error(str), sass_value_resnum));
}

PHP_FUNCTION(sass_make_warning)
{
    char *str;
    size_t str_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &str_len) == FAILURE) {
        RETURN_FALSE;
    }

    ZVAL_RES(return_value, zend_register_resource((void*)sass_make_warning(str), sass_value_resnum));
}

/* --------------------------------------------------------------
 * EXCEPTION HANDLING
 * ------------------------------------------------------------ */

zend_class_entry *sass_get_exception_base(TSRMLS_D)
{
    return zend_exception_get_default(TSRMLS_C);
}

/* --------------------------------------------------------------
 * PHP EXTENSION INFRASTRUCTURE
 * ------------------------------------------------------------ */

ZEND_BEGIN_ARG_INFO(arginfo_sass_void, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_compile, 0, 0, 1)
    ZEND_ARG_INFO(0, sass_string)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_compileFile, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setStyle, 0, 0, 1)
    ZEND_ARG_INFO(0, style)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setIncludePath, 0, 0, 1)
    ZEND_ARG_INFO(0, include_path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setPrecision, 0, 0, 1)
    ZEND_ARG_INFO(0, precision)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setComments, 0, 0, 1)
    ZEND_ARG_INFO(0, comments)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setIndent, 0, 0, 1)
    ZEND_ARG_INFO(0, indent)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setEmbed, 0, 0, 1)
    ZEND_ARG_INFO(0, map_embed)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setMapPath, 0, 0, 1)
    ZEND_ARG_INFO(0, map_path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setMapRoot, 0, 0, 1)
    ZEND_ARG_INFO(0, map_root)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setImporter, 0, 0, 1)
    ZEND_ARG_CALLABLE_INFO(0, importer, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_setFunctions, 0, 0, 1)
    ZEND_ARG_ARRAY_INFO(0, function_table, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_boolean, 0, 0, 1)
    ZEND_ARG_INFO(0, boolval)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_string, 0, 0, 1)
    ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_qstring, 0, 0, 1)
    ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_number, 0, 0, 2)
    ZEND_ARG_INFO(0, number)
    ZEND_ARG_INFO(0, unit)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_color, 0, 0, 4)
    ZEND_ARG_INFO(0, r)
    ZEND_ARG_INFO(0, g)
    ZEND_ARG_INFO(0, b)
    ZEND_ARG_INFO(0, a)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_list, 0, 0, 3)
    ZEND_ARG_INFO(0, list)
    ZEND_ARG_INFO(0, separator)
    ZEND_ARG_INFO(0, bracketed)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_map, 0, 0, 1)
    ZEND_ARG_INFO(0, map)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_error, 0, 0, 1)
    ZEND_ARG_INFO(0, message)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_sass_make_warning, 0, 0, 1)
    ZEND_ARG_INFO(0, message)
ZEND_END_ARG_INFO()


zend_function_entry sass_methods[] = {
    PHP_ME(Sass,  __construct,       arginfo_sass_void,           ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Sass,  compile,           arginfo_sass_compile,        ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  compileFile,       arginfo_sass_compileFile,    ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getStyle,          arginfo_sass_void,           ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setStyle,          arginfo_sass_setStyle,       ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getIncludePath,    arginfo_sass_void,           ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setIncludePath,    arginfo_sass_setIncludePath, ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getPrecision,      arginfo_sass_void,           ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setPrecision,      arginfo_sass_setPrecision,   ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getComments,       arginfo_sass_void,           ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setComments,       arginfo_sass_setComments,    ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getIndent,         arginfo_sass_void,           ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setIndent,         arginfo_sass_setIndent,      ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getEmbed,          arginfo_sass_void,           ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setEmbed,          arginfo_sass_setEmbed,       ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getMapPath,        arginfo_sass_void,           ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setMapPath,        arginfo_sass_setMapPath,     ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getMapRoot,        arginfo_sass_void,           ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setMapRoot,        arginfo_sass_setMapRoot,     ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setImporter,       arginfo_sass_setImporter,    ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  setFunctions,      arginfo_sass_setFunctions,   ZEND_ACC_PUBLIC)
    PHP_ME(Sass,  getLibraryVersion, arginfo_sass_void,           ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_MALIAS(Sass, compile_file, compileFile, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

zend_function_entry sass_functions[] = {
    PHP_FE(sass_make_null,    arginfo_sass_void)
    PHP_FE(sass_make_boolean, arginfo_sass_make_boolean)
    PHP_FE(sass_make_string,  arginfo_sass_make_string)
    PHP_FE(sass_make_qstring, arginfo_sass_make_qstring)
    PHP_FE(sass_make_number,  arginfo_sass_make_number)
    PHP_FE(sass_make_color,   arginfo_sass_make_color)
    PHP_FE(sass_make_list,    arginfo_sass_make_list)
    PHP_FE(sass_make_map,     arginfo_sass_make_map)
    PHP_FE(sass_make_error,   arginfo_sass_make_error)
    PHP_FE(sass_make_warning, arginfo_sass_make_warning)
};

static PHP_MINIT_FUNCTION(sass)
{
    zend_class_entry ce;
    zend_class_entry exception_ce;
    zend_class_entry function_exception_ce;
    zend_class_entry value_exception_ce;

    INIT_CLASS_ENTRY(ce, "Sass", sass_methods);

    sass_ce = zend_register_internal_class(&ce TSRMLS_CC);
    sass_ce->create_object = sass_create_handler;

    memcpy(&sass_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    sass_handlers.offset = XtOffsetOf(struct sass_object, zo);
    sass_handlers.free_obj = sass_free_storage;
    sass_handlers.clone_obj = NULL;

    INIT_CLASS_ENTRY(exception_ce, "SassException", NULL);
    INIT_CLASS_ENTRY(function_exception_ce, "SassFunctionException", NULL);
    INIT_CLASS_ENTRY(value_exception_ce, "SassValueException", NULL);

    sass_exception_ce = zend_register_internal_class_ex(&exception_ce, sass_get_exception_base(TSRMLS_C));
    sass_function_exception_ce = zend_register_internal_class_ex(&function_exception_ce, sass_get_exception_base(TSRMLS_C));
    sass_value_exception_ce = zend_register_internal_class_ex(&value_exception_ce, sass_get_exception_base(TSRMLS_C));

    #define REGISTER_SASS_CLASS_CONST_LONG(name, value) zend_declare_class_constant_long(sass_ce, ZEND_STRS( #name ) - 1, value TSRMLS_CC)

    REGISTER_SASS_CLASS_CONST_LONG(STYLE_NESTED, SASS_STYLE_NESTED);
    REGISTER_SASS_CLASS_CONST_LONG(STYLE_EXPANDED, SASS_STYLE_EXPANDED);
    REGISTER_SASS_CLASS_CONST_LONG(STYLE_COMPACT, SASS_STYLE_COMPACT);
    REGISTER_SASS_CLASS_CONST_LONG(STYLE_COMPRESSED, SASS_STYLE_COMPRESSED);

    REGISTER_STRING_CONSTANT("SASS_FLAVOR", SASS_FLAVOR, CONST_CS | CONST_PERSISTENT);

    sass_value_resnum = zend_register_list_destructors_ex(sass_value_dtor, NULL, "Sass_Value", module_number);

    return SUCCESS;
}

static PHP_MINFO_FUNCTION(sass)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "sass support", "enabled");
    php_info_print_table_row(2, "version", SASS_VERSION);
    php_info_print_table_row(2, "flavor", SASS_FLAVOR);
    php_info_print_table_row(2, "libsass version", libsass_version());
    php_info_print_table_end();
}

static zend_module_entry sass_module_entry = {
    STANDARD_MODULE_HEADER,
    "sass",
    sass_functions,
    PHP_MINIT(sass),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(sass),
    SASS_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SASS
ZEND_GET_MODULE(sass)
#endif
