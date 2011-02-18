//
//
//

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libsoup/soup.h>

#define LSID "dbcbcde3-c008-4730-84fa-7206c9224a73"

typedef struct dim_t dim_t;
struct dim_t {
	int cx;
	int cy;
};

// global options
int g_nEnabled = 1;
int g_nSchedule = 5;

dim_t get_shrink_size(dim_t sz);
dim_t get_thumb_size(dim_t sz);

static void snap(void);
static int settings(void);

void parse_options_from_json(const char *json)
{
	const char *p = strstr(json, "enabled");
	if (p != NULL)
		g_nEnabled = atoi(p + 7 + 1 + 1); // enabled + " + :
	p = strstr(json, "schedule");
	if (p != NULL)
		g_nSchedule = atoi(p + 8 + 1 + 1); // schedule + " + :
	if (g_nSchedule == 0)
		g_nSchedule = 5; //may happen if atoi sees a string not a number
}

static int settings(void)
{
	SoupSession *ss = soup_session_sync_new();
	SoupMessage *msg = soup_message_new("GET", "http://snoopon.me/settings/" LSID);
	guint status = soup_session_send_message(ss, msg);

	if (SOUP_STATUS_IS_SUCCESSFUL(status))
	{
		SoupBuffer *sb = soup_message_body_flatten(msg->response_body);
		parse_options_from_json(sb->data);
		soup_buffer_free(sb);
		return 1;
	}

	return 0;
}

static void snap_cb(GtkWidget *widget, gpointer data)
{
	snap();
}

static gboolean on_timeout(gpointer data)
{
	if (settings())
	{
		if (g_nEnabled)
			snap();
	}

	g_timeout_add_seconds(g_nSchedule*60, on_timeout, 0);
	return FALSE;
}

static void snap(void)
{
	GdkWindow *root;
	GdkPixbuf *screenshot;

	root = gdk_get_default_root_window();
	dim_t sz;
	gdk_drawable_get_size(root, &sz.cx, &sz.cy);

	screenshot = gdk_pixbuf_get_from_drawable(0, root, 0,
			0, 0, 0, 0,
			sz.cx, sz.cy);

	dim_t ssz = get_shrink_size(sz);
	dim_t tsz = get_thumb_size(sz);

	GdkPixbuf *shot1 = gdk_pixbuf_scale_simple(screenshot,
		ssz.cx, ssz.cy, GDK_INTERP_HYPER);
	GdkPixbuf *shot2 = gdk_pixbuf_scale_simple(screenshot,
		tsz.cx, tsz.cy, GDK_INTERP_HYPER);

	gchar *snapshot_data;
	gsize snapshot_size;
	gchar *snapshot2_data;
	gsize snapshot2_size;
	gchar *thumbnail_data;
	gsize thumbnail_size;

	gdk_pixbuf_save_to_buffer(screenshot,
		   	&snapshot_data, &snapshot_size, "png", 0); 
	gdk_pixbuf_save_to_buffer(shot1,
		   	&snapshot2_data, &snapshot2_size, "png", 0); 
	gdk_pixbuf_save_to_buffer(shot2,
		   	&thumbnail_data, &thumbnail_size, "png", 0); 

	gchar *snapshot_b64 = g_base64_encode(snapshot_data, snapshot_size);
	gchar *snapshot2_b64 = g_base64_encode(snapshot2_data, snapshot2_size);
	gchar *thumbnail_b64 = g_base64_encode(thumbnail_data, thumbnail_size);

	gchar *req_json = g_strdup_printf("{\"ls_id\":\"" LSID "\",\"screens\":["
			"{\"orig_width\":%d,\"orig_height\":%d,"
			"\"width\":%d,\"height\":%d,"
			"\"width2\":%d,\"height2\":%d,"
			"\"thumb_width\":%d,\"thumb_height\":%d,"
			"\"snapshot\":\"%s\",\"snapshot2\":\"%s\",\"thumbnail\":\"%s\"}]}",
				sz.cx, sz.cy,
				ssz.cx, ssz.cy,
				ssz.cx / 2, ssz.cy / 2,
				tsz.cx, tsz.cy,
				snapshot_b64, snapshot2_b64, thumbnail_b64);

	SoupSession *ss = soup_session_sync_new();
	SoupMessage *msg = soup_message_new("POST", "http://snoopon.me/captures");
	soup_message_set_request(msg, "application/json", SOUP_MEMORY_TAKE,
			req_json, strlen(req_json));
	soup_session_send_message(ss, msg);
	g_print("Screenshot sent: status=%d\n", msg->status_code);

	g_free(snapshot_b64);
	g_free(snapshot2_b64);
	g_free(thumbnail_b64);
	
	g_free(snapshot_data);
	g_free(snapshot2_data);
	g_free(thumbnail_data);
}

static void destroy_cb(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

int main(int ac, char *av[])
{
	GtkWidget *window;
	GtkWidget *button;
	//GtkWidget *btn2;

	gtk_init(&ac, &av);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(window, "destroy", G_CALLBACK(destroy_cb), 0);

	gtk_container_set_border_width(GTK_CONTAINER(window), 50);

	button = gtk_button_new_with_label("Snap!");
	g_signal_connect(button, "clicked", G_CALLBACK(snap_cb), 0);

	gtk_container_add(GTK_CONTAINER(window), button);
	//gtk_container_add(GTK_CONTAINER(window), btn2);

	gtk_widget_show(button);
	//gtk_widget_show(btn2);
	gtk_widget_show(window);

	g_timeout_add_seconds(g_nSchedule*60, on_timeout, 0);

	gtk_main();
	return 0;
}

dim_t get_shrink_size(dim_t sz)
{
	dim_t result;

	// Standard sizes:
	const int w1 = 1024; // -> 320x240
	const int h1 = 768;
	const int w2 = 1280; // -> 320x256
	const int h2 = 1024;
	const int w3 = 1440; // -> 320x200
	const int h3 = 900;

	if (sz.cx == w1 && sz.cy == h1)
	{
		result.cx = 320;
		result.cy = 240;
	}
	else if (sz.cx == w2 && sz.cy == h2)
	{
		result.cx = 320;
		result.cy = 256;
	}
	else if (sz.cx == w3 && sz.cy == h3)
	{
		result.cx = 320;
		result.cy = 200;
	}
	else
	{
		double aspect = (double)sz.cx/sz.cy;
		double d1 = fabs(aspect - (double)w1/h1);
		double d2 = fabs(aspect - (double)w2/h2);
		double d3 = fabs(aspect - (double)w3/h3);

		if (d1 <= d2 && d1 <= d3)
		{
			result.cx = 320;
			result.cy = 240;
		}
		else if (d2 <= d3)
		{
			result.cx = 320;
			result.cy = 256;
		}
		else
		{
			result.cx = 320;
			result.cy = 200;
		}
	}

	return result;
}

dim_t get_thumb_size(dim_t sz)
{
	dim_t result;

	// Standard sizes:
	const int w1 = 1024; // -> 320x240
	const int h1 = 768;
	const int w2 = 1280; // -> 320x256
	const int h2 = 1024;
	const int w3 = 1440; // -> 320x200
	const int h3 = 900;

	if (sz.cx == w1 && sz.cy == h1)
	{
		result.cx = 80;
		result.cy = 60;
	}
	else if (sz.cx == w2 && sz.cy == h2)
	{
		result.cx = 80;
		result.cy = 64;
	}
	else if (sz.cx == w3 && sz.cy == h3)
	{
		result.cx = 80;
		result.cy = 50;
	}
	else
	{
		double aspect = (double)sz.cx/sz.cy;
		double d1 = fabs(aspect - (double)w1/h1);
		double d2 = fabs(aspect - (double)w2/h2);
		double d3 = fabs(aspect - (double)w3/h3);

		if (d1 <= d2 && d1 <= d3)
		{
			result.cx = 80;
			result.cy = 60;
		}
		else if (d2 <= d3)
		{
			result.cx = 80;
			result.cy = 64;
		}
		else
		{
			result.cx = 80;
			result.cy = 50;
		}
	}

	return result;
}

//EOF
