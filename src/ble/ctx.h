/*!
 * @file ctx.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Shorter macros for Nordic's BLE connection contexts.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_BLE_CTX_H_
#define SPV_BLE_CTX_H_

#include <bluetooth/conn_ctx.h>


/**
 * @brief Obtains the context associated with this connection and declares
 * a pointer to it with name `_out_var_name`. If an error is encountered,
 * `BT_ATT_ERR_UNLIKELY` is automatically returned.
 *
 * @warning The caller must use `SPV_BT_CTX_RELEASE` when they are done using
 * the context.
 */
#define SPV_BT_CTX_GET_OR_ERR(_out_var_name, _ctx_name, _conn, _ctx_type)		\
	_ctx_type *_out_var_name = bt_conn_ctx_get(&_ctx_name##_ctx_lib, _conn);	\
	if(!_out_var_name)															\
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY)								\


/**
 * @brief Same as `SPV_BT_CTX_GET_OR_ERR` but for void functions.
 *
 * @warning The caller must use `SPV_BT_CTX_RELEASE` when they are done using
 * the context.
 */
#define SPV_BT_CTX_GET_OR_RET(_out_var_name, _ctx_name, _conn, _ctx_type)		\
	_ctx_type *_out_var_name = bt_conn_ctx_get(&_ctx_name##_ctx_lib, _conn);	\
	if(!_out_var_name)															\
			return																\



/**
 * @brief Must be called after a context is no longer required.
 */
#define SPV_BT_CTX_RELEASE(_var_name, _ctx_name) bt_conn_ctx_release(&_ctx_name##_ctx_lib, _var_name)


/**
 * @brief Generates a `BT_CONN_CTX` for a `_ctx_type`, together with automatic
 * `.connected` and `.disconnected` callbacks for initialization and freeing and
 * automatic cleanup functions.
 */
#define SPV_BT_CONN_CTX_DEF_CB(_name, _max_clients, _ctx_type, _on_conn, _on_disconn) \
																				\
	BT_CONN_CTX_DEF(_name, _max_clients, sizeof(_ctx_type)); 					\
																				\
	static void _name##__on_connected(struct bt_conn *conn, uint8_t err) { 		\
		if(!err) {																\
			void *ctx = bt_conn_ctx_alloc(&_name##_ctx_lib, conn); 				\
			if(!ctx) {															\
				return;															\
			}																	\
			memset(ctx, 0, sizeof(_ctx_type));									\
			if(NULL != (void*)_on_conn) {										\
				int (*cb_conn)(struct bt_conn*, _ctx_type*) = (void *)(_on_conn);\
				int err = cb_conn(conn, ctx);									\
				if(err) {														\
					bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);	\
				}																\
			}																	\
			bt_conn_ctx_release(&_name##_ctx_lib, ctx);							\
		}																		\
	} 																			\
																				\
	static void _name##__on_disconnected(struct bt_conn *conn, uint8_t reason) {\
		if(NULL != (void*)_on_disconn) {										\
			_ctx_type *ctx = bt_conn_ctx_get(&_name##_ctx_lib, conn);			\
			if(ctx) {															\
				void (*cb_dis)(struct bt_conn*, uint8_t, _ctx_type*) = (void *)(_on_disconn);\
				cb_dis(conn, reason, ctx);										\
				bt_conn_ctx_release(&_name##_ctx_lib, ctx);						\
			}																	\
		}																		\
		bt_conn_ctx_free(&_name##_ctx_lib, conn); 								\
	} 																			\
																				\
	BT_CONN_CB_DEFINE(_name##__ctx_conn_callbacks) = { 							\
		.connected = _name##__on_connected, 									\
		.disconnected = _name##__on_disconnected, 								\
	}

/**
 * @brief Same as `SPV_BT_CONN_CTX_DEF_CB` but without custom callbacks.
 */
#define SPV_BT_CONN_CTX_DEF(_name, _max_clients, _ctx_type) SPV_BT_CONN_CTX_DEF_CB(_name, _max_clients, _ctx_type, NULL, NULL)

#endif
