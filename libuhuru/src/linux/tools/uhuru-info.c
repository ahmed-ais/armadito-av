#include "utils/getopt.h"
#include "daemon/ipc.h"
#include "daemon/tcpsock.h"
#include "daemon/unixsock.h"

#include <assert.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct info_options {
  enum {
    TCP_SOCKET,
    UNIX_SOCKET,
  } socket_type;
  const char *unix_path;
  unsigned short port_number;
  int output_xml;
};

struct info {
};

static struct opt info_opt_defs[] = {
  { .long_form = "help", .short_form = 'h', .need_arg = 0, .is_set = 0, .value = NULL},
  { .long_form = "xml", .short_form = 'x', .need_arg = 0, .is_set = 0, .value = NULL},
  { .long_form = "tcp", .short_form = 't', .need_arg = 0, .is_set = 0, .value = NULL},
  { .long_form = "port", .short_form = 'p', .need_arg = 1, .is_set = 0, .value = NULL},
  { .long_form = "unix", .short_form = 'u', .need_arg = 0, .is_set = 0, .value = NULL},
  { .long_form = "path", .short_form = 'a', .need_arg = 1, .is_set = 0, .value = NULL},
  { .long_form = NULL, .short_form = 0, .need_arg = 0, .is_set = 0, .value = NULL},
};

static void usage(void)
{
  fprintf(stderr, "usage: uhuru-info [options]\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Uhuru antivirus information\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  --help  -h               print help and quit\n");
  fprintf(stderr, "  --tcp -t | --unix -u     use TCP (--tcp) or unix (--unix) socket (default is unix)\n");
  fprintf(stderr, "  --port=PORT | -p PORT    TCP port number\n");
  fprintf(stderr, "  --path=PATH | -a PATH    unix socket path\n");
  fprintf(stderr, "  --xml -x                 output information as XML\n");
  fprintf(stderr, "\n");

  exit(1);
}

static void parse_options(int argc, const char *argv[], struct info_options *opts)
{
  int r = opt_parse(info_opt_defs, argc, argv);
  const char *s_port;

  if (r < 0|| r >= argc)
    usage();

  if (opt_is_set(info_opt_defs, "help"))
      usage();

  if (opt_is_set(info_opt_defs, "tcp") && opt_is_set(info_opt_defs, "unix"))
    usage();

  if (opt_is_set(info_opt_defs, "help"))
      usage();

  if (opt_is_set(info_opt_defs, "tcp") && opt_is_set(info_opt_defs, "unix"))
    usage();

  opts->socket_type = UNIX_SOCKET;
  if (opt_is_set(info_opt_defs, "tcp"))
    opts->socket_type = TCP_SOCKET;

  s_port = opt_value(info_opt_defs, "port", DEFAULT_TCP_PORT);
  opts->port_number = (unsigned short)atoi(s_port);

  opts->unix_path = opt_value(info_opt_defs, "path", DEFAULT_SOCKET_PATH);

  opts->output_xml = opt_is_set(info_opt_defs, "xml");
}

static struct info *info_new(void)
{
}

static void info_save_to_stdout(struct info *info)
{
}

static void info_free(struct info *info)
{
}


static xmlDocPtr info_doc_new(void)
{
  xmlDocPtr doc;
  xmlNodePtr root_node;

  LIBXML_TEST_VERSION;

  doc = xmlNewDoc("1.0");
  root_node = xmlNewNode(NULL, "uhuru-info");
#if 0
  xmlNewProp(root_node, "xmlns", "http://www.uhuru-am.com/UpdateInfoSchema");
  xmlNewProp(root_node, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
  xmlNewProp(root_node, "xsi:schemaLocation", "http://www.uhuru-am.com/UpdateInfoSchema UpdateInfoSchema.xsd ");
#endif
  xmlDocSetRootElement(doc, root_node);

  return doc;
}

#if 0
static const char *update_status_str(enum uhuru_update_status status)
{
  switch(status) {
  case UHURU_UPDATE_NON_AVAILABLE:
    return "non available";
  case UHURU_UPDATE_OK:
    return "ok";
  case UHURU_UPDATE_LATE:
    return "late";
  case UHURU_UPDATE_CRITICAL:
    return "critical";
  }

  return "non available";
}

static void info_doc_add_module(xmlDocPtr doc, struct uhuru_module_info *info)
{
  xmlNodePtr root_node, module_node, base_node, date_node;
  struct uhuru_base_info **pinfo;
  char buffer[64];

  root_node = xmlDocGetRootElement(doc);

  module_node = xmlNewChild(root_node, NULL, "module", NULL);
  xmlNewProp(module_node, "name", info->name);

  xmlNewChild(module_node, NULL, "update-status", update_status_str(info->mod_status));

  date_node = xmlNewChild(module_node, NULL, "update-date", info->update_date);
  xmlNewProp(date_node, "type", "xs:dateTime");

  for(pinfo = info->base_infos; *pinfo != NULL; pinfo++) {
    base_node = xmlNewChild(module_node, NULL, "base", NULL);
    xmlNewProp(base_node, "name", (*pinfo)->name);

    date_node = xmlNewChild(base_node, NULL, "date", (*pinfo)->date);
    xmlNewProp(date_node, "type", "xs:dateTime");

    xmlNewChild(base_node, NULL, "version", (*pinfo)->version);
    sprintf(buffer, "%d", (*pinfo)->signature_count);
    xmlNewChild(base_node, NULL, "signature-count", buffer);
    xmlNewChild(base_node, NULL, "full-path", (*pinfo)->full_path);
  }
}

static void info_doc_add_global(xmlDocPtr doc, enum uhuru_update_status global_update_status)
{
  xmlNodePtr root_node = xmlDocGetRootElement(doc);

  xmlNewChild(root_node, NULL, "update-status", update_status_str(global_update_status));
}
#endif

static void info_doc_save_to_fd(xmlDocPtr doc, int fd)
{
  xmlSaveCtxtPtr xmlCtxt = xmlSaveToFd(fd, "UTF-8", XML_SAVE_FORMAT);

  if (xmlCtxt != NULL) {
    xmlSaveDoc(xmlCtxt, doc);
    xmlSaveClose(xmlCtxt);
  }
}

static void info_doc_free(xmlDocPtr doc)
{
  xmlFreeDoc(doc);
}

static void info_save_to_xml(struct info *info)
{
  xmlDocPtr doc = info_doc_new();

#if 0
  info_doc_add_global(doc, info->global_status);

  if (info->module_infos != NULL) {
    struct uhuru_module_info **m;

    for(m = info->module_infos; *m != NULL; m++)
      info_doc_add_module(doc, *m);
  }
#endif

  info_doc_save_to_fd(doc, STDOUT_FILENO);
  info_doc_free(doc);
}

static void ipc_handler_info_module(struct ipc_manager *m, void *data)
{
#if 0
  struct ipc_handler_info_data *handler_data = (struct ipc_handler_info_data *)data;
  struct uhuru_module_info *mod_info = g_new0(struct uhuru_module_info, 1);
  int n_base, argc;
  char *mod_name, *update_date;

  ipc_manager_get_arg_at(m, 0, IPC_STRING_T, &mod_name);
  mod_info->name = os_strdup(mod_name);
  ipc_manager_get_arg_at(m, 1, IPC_INT32_T, &mod_info->mod_status);
  ipc_manager_get_arg_at(m, 2, IPC_STRING_T, &update_date);
  mod_info->update_date = os_strdup(update_date);

  n_bases = (ipc_manager_get_argc(m) - 3) / 5;

  mod_info->base_infos = g_new0(struct uhuru_base_info *, n_bases + 1);

  for (argc = 3, n_base = 0; argc < ipc_manager_get_argc(m); argc += 5, n_base++) {
    struct uhuru_base_info *base_info = g_new(struct uhuru_base_info, 1);
    char *name, *date, *version, *full_path;

    ipc_manager_get_arg_at(m, argc+0, IPC_STRING_T, &name);
    base_info->name = os_strdup(name);
    ipc_manager_get_arg_at(m, argc+1, IPC_STRING_T, &date);
    base_info->date = os_strdup(date);
    ipc_manager_get_arg_at(m, argc+2, IPC_STRING_T, &version);
    base_info->version = os_strdup(version);
    ipc_manager_get_arg_at(m, argc+3, IPC_INT32_T, &base_info->signature_count);
    ipc_manager_get_arg_at(m, argc+4, IPC_STRING_T, &full_path);
    base_info->full_path = os_strdup(full_path);

    mod_info->base_infos[n_base] = base_info;
  }

  g_array_append_val(handler_data->g_module_infos, mod_info);
#endif
}

static void ipc_handler_info_end(struct ipc_manager *m, void *data)
{
#if 0
  struct ipc_handler_info_data *handler_data = (struct ipc_handler_info_data *)data;
  struct uhuru_info *info = handler_data->info;
  GArray *g_module_infos = handler_data->g_module_infos;

  info->module_infos = (struct uhuru_module_info **)g_module_infos->data;
  g_array_free(g_module_infos, FALSE);

  ipc_manager_get_arg_at(m, 0, IPC_INT32_T, &info->global_status);
#endif
}

static void do_info(struct info_options *opts, int client_sock)
{
  struct info *info = info_new();
  struct ipc_manager *manager;
  
  manager = ipc_manager_new(client_sock);

  ipc_manager_add_handler(manager, IPC_MSG_ID_INFO_MODULE, ipc_handler_info_module, info);
  ipc_manager_add_handler(manager, IPC_MSG_ID_INFO_END, ipc_handler_info_end, info);

  ipc_manager_msg_send(manager, IPC_MSG_ID_INFO, IPC_NONE_T);

  while (ipc_manager_receive(manager) > 0)
    ;

  ipc_manager_free(manager);

  if (opts->output_xml)
    info_save_to_xml(info);
  else
    info_save_to_stdout(info);

  info_free(info);
}

int main(int argc, const char **argv)
{
  struct info_options *opts = (struct info_options *)malloc(sizeof(struct info_options));
  int client_sock;

  parse_options(argc, argv, opts);

  if (opts->socket_type == TCP_SOCKET)
    client_sock = tcp_client_connect("127.0.0.1", opts->port_number, 10);
  else
    client_sock = unix_client_connect(opts->unix_path, 10);

  if (client_sock < 0) {
    fprintf(stderr, "cannot open client socket (errno %d)\n", errno);
    return 1;
  }

  do_info(opts, client_sock);

  return 0;
}
