/*
 *  Copyright 2014 The Luvit Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#include "luv.h"

static void shutdown_cb(uv_shutdown_t* req, int status) {
  lua_State* L = req->data;
  cleanup_udata(L, req);
  resume_with_status(L, status, 0);
}

static int shutdown_req(lua_State* L) {
  uv_shutdown_t* req = lua_newuserdata(L, sizeof(*req));
  req->type = UV_SHUTDOWN;
  setup_udata(L, req, "uv_req");
  return 1;
}

static int luv_shutdown(lua_State* L) {
  uv_shutdown_t* req = luv_check_shutdown(L, 1);
  uv_stream_t* handle = luv_check_stream(L, 2);
  int ret;
  req->data = L;
  ret = uv_shutdown(req, handle, shutdown_cb);
  if (ret < 0) {
    lua_pop(L, 1);
    return luv_error(L, ret);
  }
  return lua_yield(L, 0);
}

static void connection_cb(uv_stream_t* handle, int status) {
  lua_State* L = (lua_State*)handle->data;
  find_udata(L, handle);
  if (status < 0) {
    fprintf(stderr, "%s: %s\n", uv_err_name(status), uv_strerror(status));
    lua_pushstring(L, uv_err_name(status));
  }
  else {
    lua_pushnil(L);
  }
  luv_emit_event(L, "onconnection", 2);
}

static int luv_listen(lua_State* L) {
  uv_stream_t* handle = luv_check_stream(L, 1);
  int backlog = luaL_checkinteger(L, 2);
  int ret;
  handle->data = L;
  ret = uv_listen(handle, backlog, connection_cb);
  if (ret < 0) return luv_error(L, ret);
  lua_pushinteger(L, ret);
  return 1;
}

static int luv_accept(lua_State* L) {
  uv_stream_t* server = luv_check_stream(L, 1);
  uv_stream_t* client = luv_check_stream(L, 2);
  int ret = uv_accept(server, client);
  if (ret < 0) return luv_error(L, ret);
  lua_pushinteger(L, ret);
  return 1;
}

static void alloc_cb(__attribute__((unused)) uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

static void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
  lua_State* L = (lua_State*)handle->data;
  find_udata(L, handle);
  if (nread >= 0) {
    lua_pushnil(L);
    lua_pushlstring(L, buf->base, nread);
  }
  free(buf->base);
  if (nread == 0) return;
  if (nread == UV_EOF) {
    lua_pushnil(L);
    lua_pushnil(L);
  }
  else if (nread < 0) {
    fprintf(stderr, "%s: %s\n", uv_err_name(nread), uv_strerror(nread));
    lua_pushstring(L, uv_err_name(nread));
  }
  luv_emit_event(L, "onread", 3);
}

static int luv_read_start(lua_State* L) {
  uv_stream_t* handle = luv_check_stream(L, 1);
  int ret;
  handle->data = L;
  ret = uv_read_start(handle, alloc_cb, read_cb);
  if (ret < 0) return luv_error(L, ret);
  lua_pushinteger(L, ret);
  return 1;
}

static int luv_read_stop(lua_State* L) {
  uv_stream_t* handle = luv_check_stream(L, 1);
  int ret = uv_read_stop(handle);
  if (ret < 0) return luv_error(L, ret);
  lua_pushinteger(L, ret);
  return 1;
}

static void write_cb(uv_write_t* req, int status) {
  lua_State* L = req->data;
  cleanup_udata(L, req);
  resume_with_status(L, status, 0);
}

static int write_req(lua_State* L) {
  uv_write_t* req = lua_newuserdata(L, sizeof(*req));
  req->type = UV_WRITE;
  setup_udata(L, req, "uv_req");
  return 1;
}

static int luv_write(lua_State* L) {
  uv_write_t* req = luv_check_write(L, 1);
  uv_stream_t* handle = luv_check_stream(L, 2);
  uv_buf_t buf;
  int ret;
  buf.base = (char*) luaL_checklstring(L, 3, &buf.len);
  req->data = L;
  ret = uv_write(req, handle, &buf, 1, write_cb);
  if (ret < 0) return luv_error(L, ret);
  return lua_yield(L, 0);
}

static int luv_write2(lua_State* L) {
  uv_write_t* req = luv_check_write(L, 1);
  uv_stream_t* handle = luv_check_stream(L, 2);
  uv_buf_t buf;
  int ret;
  uv_stream_t* send_handle;
  buf.base = (char*) luaL_checklstring(L, 3, &buf.len);
  send_handle = luv_check_stream(L, 4);
  req->data = L;
  ret = uv_write2(req, handle, &buf, 1, send_handle, write_cb);
  if (ret < 0) return luv_error(L, ret);
  return lua_yield(L, 0);
}

static int luv_try_write(lua_State* L) {
  uv_stream_t* handle = luv_check_stream(L, 1);
  uv_buf_t buf;
  int ret;
  buf.base = (char*) luaL_checklstring(L, 2, &buf.len);
  ret = uv_try_write(handle, &buf, 1);
  if (ret < 0) return luv_error(L, ret);
  lua_pushinteger(L, ret);
  return 1;
}

static int luv_is_readable(lua_State* L) {
  uv_stream_t* handle = luv_check_stream(L, 1);
  lua_pushboolean(L, uv_is_readable(handle));
  return 1;
}

static int luv_is_writable(lua_State* L) {
  uv_stream_t* handle = luv_check_stream(L, 1);
  lua_pushboolean(L, uv_is_writable(handle));
  return 1;
}

static int luv_stream_set_blocking(lua_State* L) {
  uv_stream_t* handle = luv_check_stream(L, 1);
  int blocking, ret;
  luaL_checktype(L, 2, LUA_TBOOLEAN);
  blocking = lua_toboolean(L, 2);
  ret = uv_stream_set_blocking(handle, blocking);
  if (ret < 0) return luv_error(L, ret);
  lua_pushinteger(L, ret);
  return 1;
}



// static void luv_after_write(uv_write_t* req, int status) {
//   lua_State* L = luv_prepare_callback(req->data);
// #ifdef LUV_STACK_CHECK
//   int top = lua_gettop(L) - 1;
// #endif
//   if (lua_isfunction(L, -1)) {
//     luv_call(L, 0, 0);
//   } else {
//     lua_pop(L, 1);
//   }

//   luv_handle_unref(L, req->handle->data);
//   free(req->data);
//   free(req);
// #ifdef LUV_STACK_CHECK
//   assert(lua_gettop(L) == top);
// #endif
// }


// static int luv_write(lua_State* L) {
// #ifdef LUV_STACK_CHECK
//   int top = lua_gettop(L);
// #endif
//   uv_stream_t* handle = luv_get_stream(L, 1);

//   uv_write_t* req = malloc(sizeof(*req));
//   luv_req_t* lreq = malloc(sizeof(*lreq));

//   req->data = (void*)lreq;

//   lreq->lhandle = handle->data;

//   /* Reference the string in the registry */
//   lua_pushvalue(L, 2);
//   lreq->data_ref = luaL_ref(L, LUA_REGISTRYINDEX);

//   /* Reference the callback in the registry */
//   lua_pushvalue(L, 3);
//   lreq->callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

//   luv_handle_ref(L, handle->data, 1);

//   if (lua_istable(L, 2)) {
//     int length, i;
//     uv_buf_t* bufs;
//     length = lua_rawlen(L, 2);
//     bufs = malloc(sizeof(uv_buf_t) * length);
//     for (i = 0; i < length; i++) {
//       size_t len;
//       const char* chunk;
//       lua_rawgeti(L, 2, i + 1);
//       chunk = luaL_checklstring(L, -1, &len);
//       bufs[i] = uv_buf_init((char*)chunk, len);
//       lua_pop(L, 1);
//     }
//     uv_write(req, handle, bufs, length, luv_after_write);
//     /* TODO: find out if it's safe to free this soon */
//     free(bufs);
//   }
//   else {
//     size_t len;
//     const char* chunk = luaL_checklstring(L, 2, &len);
//     uv_buf_t buf = uv_buf_init((char*)chunk, len);
//     uv_write(req, handle, &buf, 1, luv_after_write);
//   }
// #ifdef LUV_STACK_CHECK
//   assert(lua_gettop(L) == top);
// #endif
//   return 0;
// }



// static int luv_is_readable(lua_State* L) {
// #ifdef LUV_STACK_CHECK
//   int top = lua_gettop(L);
// #endif
//   uv_stream_t* handle = luv_get_stream(L, 1);
//   lua_pushboolean(L, uv_is_readable(handle));
// #ifdef LUV_STACK_CHECK
//   assert(lua_gettop(L) == top + 1);
// #endif
//   return 1;
// }

// static int luv_is_writable(lua_State* L) {
// #ifdef LUV_STACK_CHECK
//   int top = lua_gettop(L);
// #endif
//   uv_stream_t* handle = luv_get_stream(L, 1);
//   lua_pushboolean(L, uv_is_writable(handle));
// #ifdef LUV_STACK_CHECK
//   assert(lua_gettop(L) == top + 1);
// #endif
//   return 1;
// }
