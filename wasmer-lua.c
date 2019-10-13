#include "wasmer.h" // wasmer/lib/runtime-c-api/wasmer.h
#include "lua.h"
#include "lauxlib.h"
#include <assert.h>
#include <stdint.h>

static int pushresult(lua_State *L, wasmer_result_t result) {
    switch(result) {
    case WASMER_OK:
        lua_pushboolean(L, true);
        return 1;
    case WASMER_ERROR:
        {
            lua_pushnil(L);
            luaL_Buffer b;
            int error_len = wasmer_last_error_length();
            assert(error_len >= 0);
            luaL_buffinitsize(L, &b, error_len);
            char *error_str = luaL_prepbuffsize(&b, error_len);
            int len = wasmer_last_error_message(error_str, error_len);
            assert(len >= 0);
            luaL_pushresultsize(&b, len);
            return 2;
        }
    default:
        assert(false);
        return 0;
    }
}

/* wasmer.instance */

static const char tname_instance[] = "wasmer.instance";

static inline wasmer_instance_t *check_instance(lua_State *L, int index) {
    return *(wasmer_instance_t **)luaL_checkudata(L, index, tname_instance);
}

static wasmer_instance_t **new_instance(lua_State *L) {
    wasmer_instance_t **ptr = (wasmer_instance_t **)lua_newuserdata(L, sizeof(wasmer_instance_t *));
    *ptr = NULL;
    luaL_getmetatable(L, tname_instance);
    lua_setmetatable(L, -2);
    return ptr;
}

static int instance_gc(lua_State *L) {
    wasmer_instance_t **instance_ptr = (wasmer_instance_t **)luaL_checkudata(L, 1, tname_instance);
    if (*instance_ptr != NULL) {
        wasmer_instance_destroy(*instance_ptr);
        *instance_ptr = NULL;
    }
    return 0;
}

/* wasmer.exports */

static const char tname_exports[] = "wasmer.exports";

static inline wasmer_exports_t *check_exports(lua_State *L, int index) {
    return *(wasmer_exports_t **)luaL_checkudata(L, index, tname_exports);
}

static wasmer_exports_t **new_exports(lua_State *L) {
    wasmer_exports_t **ptr = (wasmer_exports_t **)lua_newuserdata(L, sizeof(wasmer_exports_t *));
    *ptr = NULL;
    luaL_getmetatable(L, tname_exports);
    lua_setmetatable(L, -2);
    return ptr;
}

static int exports_gc(lua_State *L) {
    wasmer_exports_t **exports_ptr = (wasmer_exports_t **)luaL_checkudata(L, 1, tname_exports);
    if (*exports_ptr != NULL) {
        wasmer_exports_destroy(*exports_ptr);
        *exports_ptr = NULL;
    }
    return 0;
}

static int export_call(lua_State *L) {
    int nargs = lua_gettop(L);
    wasmer_export_func_t *func = (wasmer_export_func_t *)lua_touserdata(L, lua_upvalueindex(1));
    assert(func);

    uint32_t params_arith = 0;
    wasmer_result_t result = wasmer_export_func_params_arity(func, &params_arith);
    assert(result == WASMER_OK);
    wasmer_value_tag *param_tags = (wasmer_value_tag *)lua_newuserdata(L, params_arith * sizeof(wasmer_value_tag));
    result = wasmer_export_func_params(func, param_tags, params_arith);
    assert(result == WASMER_OK);
    wasmer_value_t *params = (wasmer_value_t *)lua_newuserdata(L, params_arith * sizeof(wasmer_value_t));

    if (nargs < params_arith) {
        return luaL_error(L, "too few arguments; expected %d, but got %d", (int)params_arith, nargs);
    }

    for (int i = 0; i < params_arith; ++i) {
        switch (param_tags[i]) {
        case WASM_I32:
            params[i].tag = WASM_I32;
            params[i].value.I32 = luaL_checkinteger(L, i + 1);
            break;
        case WASM_I64:
            params[i].tag = WASM_I64;
            params[i].value.I64 = luaL_checkinteger(L, i + 1);
            break;
        case WASM_F32:
            params[i].tag = WASM_F32;
            params[i].value.F32 = luaL_checknumber(L, i + 1);
            break;
        case WASM_F64:
            params[i].tag = WASM_F64;
            params[i].value.F64 = luaL_checknumber(L, i + 1);
            break;
        default:
            assert(false);
        }
    }

    uint32_t returns_arith = 0;
    result = wasmer_export_func_returns_arity(func, &returns_arith);
    assert(result == WASMER_OK);
    wasmer_value_t *results = (wasmer_value_t *)lua_newuserdata(L, returns_arith * sizeof(wasmer_value_t));

    result = wasmer_export_func_call(func, params, params_arith, results, returns_arith);
    if (result == WASMER_OK) {
        for (uint32_t i = 0; i < returns_arith; ++i) {
            switch (results[i].tag) {
            case WASM_I32:
                lua_pushinteger(L, results[i].value.I32);
                break;
            case WASM_I64:
                lua_pushinteger(L, results[i].value.I64);
                break;
            case WASM_F32:
                lua_pushnumber(L, results[i].value.F32);
                break;
            case WASM_F64:
                lua_pushnumber(L, results[i].value.F64);
                break;
            default:
                assert(false && "invalid result tag");
                break;
            }
        }
        return returns_arith;
    } else {
        return pushresult(L, result);
    }
}

/* instance:exports() */
static int instance_exports(lua_State *L) {
    wasmer_instance_t *instance = check_instance(L, 1);
    wasmer_exports_t **exports_ptr = new_exports(L);
    wasmer_instance_exports(instance, exports_ptr);
    wasmer_exports_t *exports = *exports_ptr;
    assert(exports != NULL);
    int n = wasmer_exports_len(exports);
    lua_createtable(L, /* array part */ 0, /* non-array part */ n);
    for (int i = 0; i < n; ++i) {
        wasmer_export_t *export = wasmer_exports_get(exports, i);
        wasmer_byte_array name = wasmer_export_name(export);
        lua_pushlstring(L, (const char *)name.bytes, name.bytes_len);
        switch (wasmer_export_kind(export)) {
        case WASM_FUNCTION:
            {
                lua_pushlightuserdata(L, (void *)wasmer_export_to_func(export));
                lua_pushvalue(L, -4); /* exports */
                lua_pushcclosure(L, export_call, 2);
                break;
            }
        case WASM_GLOBAL:
            {
                lua_pushliteral(L, "<global>");
                break;
            }
        case WASM_MEMORY:
            {
                lua_pushliteral(L, "<memory>");
                break;
            }
        case WASM_TABLE:
            {
                lua_pushliteral(L, "<table>");
                break;
            }
        default:
            assert(false);
        }
        lua_settable(L, -3);
    }
    return 1;
}

// instantiate(wasm: string, import_object: table) -> instantiate | nil, error
static int lwasmer_instantiate(lua_State *L) {
    size_t bytes_len = 0;
    const char *bytes = luaL_checklstring(L, 1, &bytes_len);
    wasmer_instance_t **instance_ptr = new_instance(L);
    wasmer_result_t result = wasmer_instantiate(instance_ptr, (uint8_t *)bytes, bytes_len, NULL, 0);
    if (result == WASMER_OK) {
        return 1;
    } else {
        return pushresult(L, result);
    }
}

static const luaL_Reg exports_meta[] = {
    {"__gc", exports_gc},
    {NULL, NULL},
};

static const luaL_Reg instance_meta[] = {
    {"__gc", instance_gc},
    {"exports", instance_exports},
    {NULL, NULL},
};

static const luaL_Reg funcs[] = {
    {"instantiate", lwasmer_instantiate},
    {NULL, NULL},
};

int luaopen_wasmer(lua_State *L) {
    luaL_newmetatable(L, tname_exports);
    luaL_setfuncs(L, exports_meta, 0);
    lua_pop(L, 1);

    luaL_newmetatable(L, tname_instance);
    luaL_setfuncs(L, instance_meta, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_newlibtable(L, funcs);
    luaL_setfuncs(L, funcs, 0);
    return 1;
}
