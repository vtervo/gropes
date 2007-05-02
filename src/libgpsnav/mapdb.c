#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <locale.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <gpsnav/gpsnav.h>
#include <gpsnav/map.h>
#include <gpsnav/coord.h>

static int ifprintf(FILE *file, int indent, const char *format, ...)
{
	va_list ap;
	int i, r;

	for (i = 0; i < indent; i++)
		fwrite("\t", 1, 1, file);
	va_start(ap, format);
	r = vfprintf(file, format, ap);
	va_end(ap);
	fputc('\n', file);
	return r;
}

static int ifprintf_val(FILE *file, int indent, const char *key,
			const char *format, ...)
{
	va_list ap;
	int i, r;

	for (i = 0; i < indent; i++)
		fwrite("\t", 1, 1, file);
	fprintf(file, "<%s>", key);
	va_start(ap, format);
	r = vfprintf(file, format, ap);
	va_end(ap);
	fprintf(file, "</%s>\n", key);
	return r;
}

int gpsnav_mapdb_write(struct gpsnav *nav, const char *filename)
{
	char *base_path, *p;
	struct gps_map *map;
	FILE *f;
	char *locale;

	f = fopen(filename, "w");
	if (f == NULL) {
		gps_error("Unable to open file '%s' for writing", filename);
		return -1;
	}

	base_path = gpsnav_get_full_path(filename, NULL);
	if (base_path == NULL) {
		fclose(f);
		return -ENOMEM;
	}
	p = strrchr(base_path, '/');
	if (p != NULL)
		*p = '\0';

	locale = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	fputs("<?xml version=\"1.0\"?>\n", f);
	fputs("<gpsnav>\n", f);
	for (map = nav->map_list.lh_first; map != NULL; map = map->entries.le_next) {
		struct gps_key_value *kv;
		int kv_count, i;

		if (gpsnav_get_provider_map_info(nav, map, &kv, &kv_count, base_path) < 0)
			continue;

		ifprintf(f, 1, "<map>");
		ifprintf_val(f, 2, "bottom-left-coord", "%.9lf, %.9lf",
			     map->area.start.la, map->area.start.lo);
		ifprintf_val(f, 2, "top-right-coord", "%.9lf, %.9lf",
			     map->area.end.la, map->area.end.lo);
		ifprintf_val(f, 2, "type", "%s", map->prov->name);

		if (kv_count) {
			ifprintf(f, 2, "<type-specific-data>");
			for (i = 0; i < kv_count; i++) {
				ifprintf_val(f, 3, kv[i].key, kv[i].value);
				free(kv[i].value);
				free(kv[i].key);
			}
			free(kv);
			ifprintf(f, 2, "</type-specific-data>");
		}

		ifprintf(f, 1, "</map>");
	}
	fputs("</gpsnav>\n", f);
	free(base_path);
	fclose(f);
	setlocale(LC_NUMERIC, locale);

	return 0;
}

static int parse_map_type_specific_data(xmlNodePtr type_data_node,
					struct gps_key_value **kv_out,
					int *kv_count)
{
	struct gps_key_value *kv;
	int count;
	xmlNodePtr cur;

	count = 0;
	for (cur = type_data_node->xmlChildrenNode; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
		count++;
	}
	if (!count) {
		*kv_out = NULL;
		*kv_count = 0;
		return 0;
	}
	kv = malloc(sizeof(*kv) * count);
	if (kv == NULL)
		goto fail;
	*kv_out = kv;
	*kv_count = count;
	for (cur = type_data_node->xmlChildrenNode; cur != NULL; cur = cur->next) {
		char *content;

		if (cur->type != XML_ELEMENT_NODE)
			continue;
		kv->key = strdup((char *) cur->name);
		if (kv->key == NULL)
			goto fail;
		content = (char *) xmlNodeGetContent(cur);
		if (content == NULL)
			goto fail;
		kv->value = strdup(content);
		xmlFree(content);
		if (kv->value == NULL)
			goto fail;
		kv++;
	}
	return 0;
fail:
	gps_error("malloc failure");
	return -ENOMEM;
}

#define VALUE_BOT_LEFT_COORD	(1 << 0)
#define VALUE_TOP_RIGHT_COORD	(1 << 1)
#define VALUE_TYPE		(1 << 2)
#define VALUE_ALL               ((1 << 0) | (1 << 1) | (1 << 2))

static int parse_map(struct gpsnav *nav, const char *base_path,
		     xmlNodePtr map_node)
{
	xmlNodePtr cur;
	struct gps_key_value *kv = NULL;
	int kv_count = 0;
	struct gps_map *map;
	char *content = NULL;
	int r, i;
	int got_value = 0;

	map = gps_map_new();
	if (map == NULL) {
		gps_error("malloc failure");
		return -ENOMEM;
	}

	for (cur = map_node->xmlChildrenNode; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
		content = (char *) xmlNodeGetContent(cur);
		if (content == NULL) {
			gps_error("xmlNodeGetContent() failed");
			goto fail;
		}
		if (xmlStrcmp(cur->name, (const xmlChar *) "bottom-left-coord") == 0) {
			if (sscanf(content, "%lf, %lf", &map->area.start.la, &map->area.start.lo) != 2) {
				gps_error("Invalid bottom-left-coord");
				goto fail;
			}
			got_value |= VALUE_BOT_LEFT_COORD;
		} else if (xmlStrcmp(cur->name, (const xmlChar *) "top-right-coord") == 0) {
			if (sscanf(content, "%lf, %lf", &map->area.end.la, &map->area.end.lo) != 2) {
				gps_error("Invalid top-right-coord");
				goto fail;
			}
			got_value |= VALUE_TOP_RIGHT_COORD;
		} else if (xmlStrcmp(cur->name, (const xmlChar *) "type") == 0) {
			map->prov = gpsnav_find_provider(nav, content);
			if (map->prov == NULL) {
				gps_error("Map type '%s' invalid", cur->content);
				goto fail;
			}
			got_value |= VALUE_TYPE;
		} else if (xmlStrcmp(cur->name, (const xmlChar *) "type-specific-data") == 0) {
			r = parse_map_type_specific_data(cur, &kv, &kv_count);
			if (r < 0)
				goto fail;
		}
		xmlFree(content);
		content = NULL;
	}

	if (got_value != VALUE_ALL) {
		gps_error("Did not get all needed values");
		return -1;
	}

	r = gpsnav_add_map(nav, map, kv, kv_count, base_path);

	for (i = 0; i < kv_count; i++) {
		free(kv[i].key);
		free(kv[i].value);
	}
	if (kv_count)
		free(kv);

	if (r)
		goto fail;

	return 0;
fail:
	if (content != NULL)
		xmlFree(content);
	free(map);
	return -1;
}

int gpsnav_mapdb_read(struct gpsnav *nav, const char *filename)
{
	char *base_path, *p;
	xmlDocPtr doc;
	xmlNodePtr cur;
	int r = -1;
	char *locale;

	/* The map path can be relative to the XML file */
	base_path = gpsnav_get_full_path(filename, NULL);
	if (base_path == NULL)
		return -ENOMEM;
	p = strrchr(base_path, '/');
	if (p != NULL)
		*p = '\0';

	doc = xmlParseFile(filename);
	if (doc == NULL) {
		gps_error("Unable to parse '%s'", filename);
		goto fail;
	}

	cur = xmlDocGetRootElement(doc);
	if (cur == NULL) {
		gps_error("Empty XML file '%s'", filename);
		goto fail;
	}

	if (xmlStrcmp(cur->name, (const xmlChar *) "gpsnav") != 0) {
		fprintf(stderr, "XML file '%s' of wrong type ('%s')",
			filename, cur->name);
	}

	r = 0;
	locale = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	for (cur = cur->xmlChildrenNode; cur != NULL; cur = cur->next) {
		if (cur->type != XML_ELEMENT_NODE)
			continue;
		if (xmlStrcmp(cur->name, (const xmlChar *) "map") != 0) {
			fprintf(stderr, "Invalid XML node '%s'", cur->name);
			continue;
		}
		r = parse_map(nav, base_path, cur);
		if (r < 0)
			break;
	}
	setlocale(LC_NUMERIC, locale);

	xmlFreeDoc(doc);
fail:
	free(base_path);
	return r;
}
