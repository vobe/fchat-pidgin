#include "f-list_ignore.h"

void flist_ignore_list_request_add(PurpleConnection *pc, const gchar *character)
{
    JsonObject *json = json_object_new();
    json_object_set_string_member(json, "action", "add");
    json_object_set_string_member(json, "character", character);
    flist_request(pc, FLIST_IGNORE, json);
    json_object_unref(json);
}

void flist_ignore_list_request_remove(PurpleConnection *pc, const gchar *character)
{
    JsonObject *json = json_object_new();
    json_object_set_string_member(json, "action", "delete");
    json_object_set_string_member(json, "character", character);
    flist_request(pc, FLIST_IGNORE, json);
    json_object_unref(json);
}

void flist_ignore_list_request_list(PurpleConnection *pc)
{
    JsonObject *json = json_object_new();
    json_object_set_string_member(json, "action", "list");
    flist_request(pc, FLIST_IGNORE, json);
    json_object_unref(json);
}

void flist_ignore_list_action(PurplePluginAction *action) {
    PurpleConnection *pc = action->context;
    g_return_if_fail(pc);
    flist_ignore_list_request_list(pc);
}

static void ignore_list_remove_cb(PurpleConnection *pc, GList *row, void *user_data)
{
    flist_ignore_list_request_remove(pc, g_list_nth_data(row, 0));
}

gboolean flist_ignore_character_is_ignored(PurpleConnection *pc, const gchar *character)
{
    FListAccount *fla = pc->proto_data;
    return g_list_find_custom(fla->ignore_list, character, (GCompareFunc)g_ascii_strcasecmp) != NULL;
}

void flist_blist_node_ignore_action(PurpleBlistNode *node, gpointer data) {
    PurpleBuddy *b = (PurpleBuddy*) node;
    PurpleAccount *pa = purple_buddy_get_account(b);
    PurpleConnection *pc;
    FListAccount *fla;
    FListIgnoreActionType type = GPOINTER_TO_INT(data);
    const gchar *name = purple_buddy_get_name(b);

    g_return_if_fail((pc = purple_account_get_connection(pa)));
    g_return_if_fail((fla = pc->proto_data));

    if (type == FLIST_NODE_IGNORE)
    {
        flist_ignore_list_request_add(pc, name);
    }
    else if (type == FLIST_NODE_UNIGNORE)
    {
        flist_ignore_list_request_remove(pc, name);
    }
}

void *close_ignore_list_cb(gpointer user_data)
{
    purple_notify_searchresults_free(user_data);
    return NULL;
}

gboolean flist_process_IGN(PurpleConnection *pc, JsonObject *root) {
    FListAccount *fla = pc->proto_data;
    const gchar *action;
    const gchar *character;

    action = json_object_get_string_member(root, "action");

    if (g_ascii_strncasecmp(action, "add", 3) == 0)
    {
        char msg[128];
        character = json_object_get_string_member(root, "character");
        fla->ignore_list = g_list_append(fla->ignore_list, g_strdup(character));
        g_snprintf( msg, sizeof( msg ), _( "'%s' has been added to your ignore list." ), character );
        purple_notify_info(pc, "User ignored!", msg, NULL);
    }
    else if (g_ascii_strncasecmp(action, "delete", 6) == 0)
    {
        char msg[128];
        character = json_object_get_string_member(root, "character");
        GList *ignored_char = g_list_find_custom(fla->ignore_list, character, (GCompareFunc)g_ascii_strcasecmp);
        
        if (ignored_char != NULL)
        {
            g_free(ignored_char->data);
            fla->ignore_list = g_list_delete_link(fla->ignore_list, ignored_char);
        }

        g_snprintf( msg, sizeof( msg ), _( "'%s' has been removed from your ignore list." ), character );
        purple_notify_info(pc, "User removed!", msg, NULL);
    }
    else if (g_ascii_strncasecmp(action, "list", 4) == 0)
    {
        // Response looks like
        //  IGN {"characters":["asdasdasdasdasdf343ddfsd","testytest","blablabla"],"action":"list"}
        
        JsonArray *chars = json_object_get_array_member(root, "characters");
        PurpleNotifySearchResults *results = purple_notify_searchresults_new();
        PurpleNotifySearchColumn *column = purple_notify_searchresults_column_new("Character");
        purple_notify_searchresults_column_add(results, column);

        for (size_t i = 0; i < json_array_get_length(chars); i++)
        {
            GList *row = NULL;
            row = g_list_append(row, (gpointer) json_array_get_string_element(chars, i));
            purple_notify_searchresults_row_add(results, row);
        }

        // XXX This button is ignored, might be a bug in libpurple/pidgin
	    purple_notify_searchresults_button_add_labeled(results, "Remove", ignore_list_remove_cb);
        purple_notify_searchresults(pc, NULL, NULL, _("Ignore List"), results, NULL, NULL);
    }
    else if (g_ascii_strncasecmp(action, "init", 4) == 0)
    {
        flist_g_list_free_full(fla->ignore_list, g_free);
        fla->ignore_list = NULL;

        JsonArray *chars = json_object_get_array_member(root, "characters");

        for (size_t i = 0; i < json_array_get_length(chars); i++)
        {
            fla->ignore_list = g_list_append(fla->ignore_list, g_strdup(json_array_get_string_element(chars, i)));
        }
    }

    return TRUE;
}

PurpleCmdRet flist_ignore_cmd(PurpleConversation *convo, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleConnection *pc = purple_conversation_get_gc(convo);
    
    if (args[0] == NULL)
        return PURPLE_CMD_RET_FAILED;

    gchar *subcmd = args[0];
    if (g_ascii_strncasecmp(subcmd, "add", 3) == 0)
    {
        if (args[1] == NULL)
            return PURPLE_CMD_RET_FAILED;

        flist_ignore_list_request_add(pc, args[1]);
    }
    else if (g_ascii_strncasecmp(subcmd, "delete", 6) == 0)
    {
        if (args[1] == NULL)
            return PURPLE_CMD_RET_FAILED;

        flist_ignore_list_request_remove(pc, args[1]);
    }
    else if (g_ascii_strncasecmp(subcmd, "list", 4) == 0)
    {
        if (args[1] != NULL)
            return PURPLE_CMD_RET_FAILED;

        flist_ignore_list_request_list(pc);
    }

    return PURPLE_CMD_RET_OK;
}

