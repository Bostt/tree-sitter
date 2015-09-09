#include <stdbool.h>
#include "runtime/node.h"
#include "runtime/length.h"
#include "runtime/tree.h"
#include "runtime/document.h"

TSNode ts_node_make(const TSTree *tree, TSLength offset) {
  return (TSNode){.data = tree, .offset = offset };
}

static inline const TSTree *ts_node__tree(TSNode this) {
  return this.data;
}

static inline TSLength ts_node__offset(TSNode this) {
  return this.offset;
}

static inline TSNode ts_node__null() {
  return ts_node_make(NULL, ts_length_zero());
}

TSLength ts_node_pos(TSNode this) {
  return ts_length_add(ts_node__offset(this), ts_node__tree(this)->padding);
}

TSLength ts_node_size(TSNode this) {
  return ts_node__tree(this)->size;
}

bool ts_node_eq(TSNode this, TSNode other) {
  return ts_tree_eq(ts_node__tree(this), ts_node__tree(other)) &&
         ts_length_eq(ts_node__offset(this), ts_node__offset(other));
}

const char *ts_node_name(TSNode this, const TSDocument *document) {
  return document->parser.language->symbol_names[ts_node__tree(this)->symbol];
}

const char *ts_node_string(TSNode this, const TSDocument *document) {
  return ts_tree_string(ts_node__tree(this),
                        document->parser.language->symbol_names);
}

TSNode ts_node_parent(TSNode this) {
  const TSTree *tree = ts_node__tree(this);
  TSLength position = ts_node__offset(this);

  do {
    TSTree *parent = tree->context.parent;
    if (!parent)
      return ts_node__null();

    for (size_t i = 0; i < tree->context.index; i++) {
      TSTree *child = parent->children[i];
      position = ts_length_sub(position, ts_tree_total_size(child));
    }

    tree = parent;
  } while (!ts_tree_is_visible(tree));

  return ts_node_make(tree, position);
}

static TSNode ts_node__child_with_type(TSNode this, size_t child_index,
                                       TSNodeType type) {
  const TSTree *tree = ts_node__tree(this);
  TSLength position = ts_node__offset(this);

  bool did_descend = true;
  while (did_descend) {
    did_descend = false;

    size_t index = 0;
    for (size_t i = 0; i < tree->child_count; i++) {
      TSTree *child = tree->children[i];
      if (child->options.type >= type) {
        if (index == child_index)
          return ts_node_make(child, position);
        index++;
      } else {
        size_t grandchild_index = child_index - index;
        size_t grandchild_count = (type == TSNodeTypeNormal)
                                    ? child->named_child_count
                                    : child->visible_child_count;
        if (grandchild_index < grandchild_count) {
          did_descend = true;
          tree = child;
          child_index = grandchild_index;
          break;
        }
        index += grandchild_count;
      }
      position = ts_length_add(position, ts_tree_total_size(child));
    }
  }

  return ts_node__null();
}

static TSNode ts_node__prev_sibling_with_type(TSNode this, TSNodeType type) {
  const TSTree *tree = ts_node__tree(this);
  TSLength position = ts_node__offset(this);

  do {
    size_t index = tree->context.index;
    position = ts_length_sub(position, tree->context.offset);
    tree = tree->context.parent;
    if (!tree)
      break;

    for (size_t i = index - 1; i + 1 > 0; i--) {
      const TSTree *child = tree->children[i];
      TSLength child_position = ts_length_add(position, child->context.offset);
      if (child->options.type >= type)
        return ts_node_make(child, child_position);
      size_t grandchild_count = (type == TSNodeTypeNormal)
                                  ? child->named_child_count
                                  : child->visible_child_count;
      if (grandchild_count > 0)
        return ts_node__child_with_type(ts_node_make(child, child_position),
                                        grandchild_count - 1, type);
    }
  } while (!ts_tree_is_visible(tree));

  return ts_node__null();
}

static TSNode ts_node__next_sibling_with_type(TSNode this, TSNodeType type) {
  const TSTree *tree = ts_node__tree(this);
  TSLength position = ts_node__offset(this);

  do {
    size_t index = tree->context.index;
    position = ts_length_sub(position, tree->context.offset);
    tree = tree->context.parent;
    if (!tree)
      break;

    for (size_t i = index + 1; i < tree->child_count; i++) {
      const TSTree *child = tree->children[i];
      TSLength child_position = ts_length_add(position, child->context.offset);
      if (child->options.type >= type)
        return ts_node_make(child, child_position);
      size_t grandchild_count = (type == TSNodeTypeNormal)
                                  ? child->named_child_count
                                  : child->visible_child_count;
      if (grandchild_count > 0)
        return ts_node__child_with_type(ts_node_make(child, child_position), 0,
                                        type);
    }
  } while (!ts_tree_is_visible(tree));

  return ts_node__null();
}

static TSNode ts_node__find_for_range_with_type(TSNode this, size_t min, size_t max, TSNodeType type) {
  const TSTree *tree = ts_node__tree(this), *last_visible_tree = tree;
  TSLength position = ts_node__offset(this), last_visible_position = position;

  bool did_descend = true;
  while (did_descend) {
    did_descend = false;

    for (size_t i = 0; i < tree->child_count; i++) {
      const TSTree *child = tree->children[i];
      if (position.chars + child->padding.chars > min)
        break;
      if (position.chars + child->padding.chars + child->size.chars > max) {
        tree = child;
        if (child->options.type >= type) {
          last_visible_tree = tree;
          last_visible_position = position;
        }
        did_descend = true;
        break;
      }
      position = ts_length_add(position, ts_tree_total_size(child));
    }
  }

  return ts_node_make(last_visible_tree, last_visible_position);
}

TSNode ts_node_next_sibling(TSNode this) {
  return ts_node__next_sibling_with_type(this, TSNodeTypeNormal);
}

TSNode ts_node_prev_sibling(TSNode this) {
  return ts_node__prev_sibling_with_type(this, TSNodeTypeNormal);
}

TSNode ts_node_next_concrete_sibling(TSNode this) {
  return ts_node__next_sibling_with_type(this, TSNodeTypeConcrete);
}

TSNode ts_node_prev_concrete_sibling(TSNode this) {
  return ts_node__prev_sibling_with_type(this, TSNodeTypeConcrete);
}

size_t ts_node_concrete_child_count(TSNode this) {
  return ts_node__tree(this)->visible_child_count;
}

bool ts_node_is_concrete(TSNode this) {
  return ts_node__tree(this)->options.type == TSNodeTypeConcrete;
}

size_t ts_node_child_count(TSNode this) {
  return ts_node__tree(this)->named_child_count;
}

TSNode ts_node_child(TSNode this, size_t child_index) {
  return ts_node__child_with_type(this, child_index, TSNodeTypeNormal);
}

TSNode ts_node_concrete_child(TSNode this, size_t child_index) {
  return ts_node__child_with_type(this, child_index, TSNodeTypeConcrete);
}

TSNode ts_node_find_for_range(TSNode this, size_t min, size_t max) {
  return ts_node__find_for_range_with_type(this, min, max, TSNodeTypeNormal);
}

TSNode ts_node_find_concrete_for_range(TSNode this, size_t min, size_t max) {
  return ts_node__find_for_range_with_type(this, min, max, TSNodeTypeConcrete);
}

TSNode ts_node_find_for_pos(TSNode this, size_t position) {
  return ts_node_find_for_range(this, position, position);
}
