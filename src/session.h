
#ifndef SRC_SESSION_H
#define SRC_SESSION_H

/*
 * session 会话
 * 一个TCP连接的基本单元
 */

#include <stdint.h>
#include <netinet/in.h>

#include "event.h"
#include "network.h"

#include "utils.h"
#include "message.h"

#define SESSION_READING			0x01	// 等待读事件, 读很忙
#define SESSION_WRITING			0x02	// 等待写事件, 写很忙
#define SESSION_KEEPALIVING		0x04	// 等待保活事件
#define SESSION_EXITING			0x10	// 等待退出, 数据全部发送完毕后, 即可终止

enum SessionType
{
	eSessionType_Once 		= 1,	// 临时会话
	eSessionType_Persist	= 2,	// 永久会话, 有断线重连的功能
};

struct session_setting
{
	int32_t timeout_msecs;
	int32_t keepalive_msecs;
	int32_t max_inbuffer_len;
};

struct session
{
	sid_t		id;

	int32_t 	fd;
	int8_t 		type;
	int8_t 		status;
	
	uint16_t	port;
	char 		host[INET_ADDRSTRLEN];

	// 读写以及超时事件
	event_t 	evread;
	event_t 	evwrite;
	event_t		evkeepalive;

	// 事件集和管理器
	evsets_t	evsets;
	void *		manager;

	// 逻辑
	void *		context;
	ioservice_t service;
	
	// 接收缓冲区
	struct buffer		inbuffer;

	// 发送队列以及消息偏移量
	int32_t 			msgoffsets;
	struct arraylist 	outmsglist;

	// 会话的设置
	struct session_setting setting;
};

// 64位SID的构成
// |XX	|XX		|XXXXXXXX	|XXXX	|
// |RES	|INDEX	|FD			|SEQ	|
// |8	|8		|32			|16		|

#define SID_MASK	0x00ffffffffffffffULL
#define KEY_MASK	0x0000ffffffff0000ULL
#define SEQ_MASK	0x000000000000ffffULL
#define INDEX_MASK	0x00ff000000000000ULL

#define SID_SEQ(sid)	( (sid)&SEQ_MASK )
#define SID_KEY(sid)	( ((sid)&KEY_MASK) >> 16 )
#define SID_INDEX(sid)	( ( ((sid)&INDEX_MASK) >> 48 ) - 1 )

// 会话初始化
int32_t session_init( struct session * self, uint32_t size );

// 会话开始
int32_t session_start( struct session * self, int8_t type, int32_t fd, evsets_t sets );

// 
void session_set_endpoint( struct session * self, char * host, uint16_t port );

// 向session发送数据
int32_t session_send( struct session * self, char * buf, uint32_t nbytes );

// 向session的发送队列中添加一条消息
int32_t session_append( struct session * self, struct message * message );

// 会话注册/反注册网络事件
void session_add_event( struct session * self, int16_t ev );
void session_del_event( struct session * self, int16_t ev );

// 开始发送心跳
int32_t session_start_keepalive( struct session * self );

// 重连远程服务器
int32_t session_start_reconnect( struct session * self );

// 尝试终止会话
// 该API会尽量把发送队列中的数据发送出去
// libevlite安全终止会话的核心模块
int32_t session_shutdown( struct session * self );

// 会话结束
int32_t session_end( struct session * self );

// 销毁会话
int32_t session_final( struct session * self );

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

struct session_entry
{
	int32_t		key;
	uint16_t	seq;
	
	struct session data;
};

struct session_manager
{
	uint8_t		index;
	
	uint32_t	size;
	uint32_t	count;
	
	// 避免cache-missing引发的性能问题
	// 性能可以达到开放地址法的hashtable
	struct arraylist * entries;
};

// 创建会话管理器
// index	- 会话管理器索引号
// count	- 会话管理器中管理多少个会话
struct session_manager * session_manager_create( uint8_t index, uint32_t size );

// 分配一个会话
// key		- 会话的描述符
struct session * session_manager_alloc( struct session_manager * self, int32_t key );

// 从会话管理器中取出一个会话
struct session * session_manager_get( struct session_manager * self, sid_t id );

// 从会话管理器中移出会话
int32_t session_manager_remove( struct session_manager * self, struct session * session );

// 销毁会话管理器
void session_manager_destroy( struct session_manager * self );

#endif

