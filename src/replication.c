#include "cluster.h"
#include "ht.h"
#include "objects.h"
#include "queue.h"
#include "server.h"
#include "util.h"
#include "vec.h"
#include <assert.h>
#include <stdio.h>

static void replicate_add_ht(Ht* ht, Client* slave);
static void replicate_add_stack(Vec* stack, Client* slave);
static void replicate_add_queue(Queue* q, Client* slave);
static void replicate_add_clusters(Cluster* cluster, Client* slave);
static void replicate_add_object(Builder* builder, Object* obj);

void replicate(LexiDB* db, Client* slave) {
    builder_add_arr(&(slave->builder), 2);
    builder_add_string(&(slave->builder), "REPLICATION", 11);
    builder_add_arr(&(slave->builder), 4);
    replicate_add_ht(db->ht, slave);
    replicate_add_stack(db->stack, slave);
    replicate_add_queue(db->queue, slave);
    replicate_add_clusters(db->cluster, slave);
}

static void replicate_add_ht(Ht* ht, Client* slave) {
    Builder* builder = &(slave->builder);
    builder_add_arr(builder, 2);
    builder_add_string(builder, "HT", 2);
    if (ht->len != 0) {
        HtEntriesIter* ht_iter;
        Entry* cur;

        ht_iter = ht_entries_iter(ht);
        assert(ht_iter != NULL);

        builder_add_arr(builder, ht->len);

        for (cur = ht_iter->cur; cur != NULL;
                ht_entries_next(ht_iter), cur = ht_iter->cur) {
            Object* obj = cur->value;
            builder_add_arr(builder, 2);
            builder_add_string(builder, (char*)(cur->key), cur->key_len);
            replicate_add_object(builder, obj);
        }
        ht_entries_iter_free(ht_iter);
    } else {
        builder_add_arr(builder, 0);
    }
}

static void replicate_add_stack(Vec* stack, Client* slave) {
    VecIter stack_iter;
    Builder* builder = &(slave->builder);
    builder_add_arr(builder, 2);
    builder_add_string(builder, "STACK", 5);
    if (stack->len != 0) {
        Object* cur;
        stack_iter = vec_iter_new(stack, 0);

        builder_add_arr(builder, stack->len);

        for (cur = stack_iter.cur; cur != NULL;
             vec_iter_next(&stack_iter), cur = stack_iter.cur) {
            replicate_add_object(builder, cur);
        }
    } else {
        builder_add_arr(builder, 0);
    }
}

static void replicate_add_queue(Queue* q, Client* slave) {
    QIter q_iter;
    Builder* builder = &(slave->builder);
    builder_add_arr(builder, 2);
    builder_add_string(builder, "QUEUE", 5);
    if (q->len != 0) {
        QNode* cur;
        q_iter = queue_iter_new(q);

        builder_add_arr(builder, q->len);
        for (cur = q_iter.cur; cur != NULL;
             queue_iter_next(&q_iter), cur = q_iter.next) {
            Object* obj = ((Object*)(cur->data));
            replicate_add_object(builder, obj);
        }
    } else {
        builder_add_arr(builder, 0);
    }
}

static void replicate_add_clusters(Cluster* cluster, Client* slave) {
    HtEntriesIter* cluster_iter;
    Builder* builder = &(slave->builder);
    builder_add_arr(builder, 2);
    builder_add_string(builder, "CLUSTER", 7);
    if (cluster->len != 0) {
        Entry* cluster_cur;
        cluster_iter = ht_entries_iter(cluster);
        assert(cluster_iter != NULL);

        builder_add_arr(builder, cluster->len);

        for (cluster_cur = cluster_iter->cur; cluster_cur != NULL;
                ht_entries_next(cluster_iter),
                cluster_cur = cluster_iter->cur) {

            ClusterDB* cdb = *((ClusterDB**)(cluster_cur->value));
            builder_add_arr(builder, 3);

            builder_add_string(builder, (char*)(cluster_cur->key), cluster_cur->key_len);

            replicate_add_ht(cdb->ht, slave);
            replicate_add_stack(cdb->vec, slave);
        }
        ht_entries_iter_free(cluster_iter);
    } else {
        builder_add_arr(builder, 0);
    }
}

static void replicate_add_object(Builder* builder, Object* obj) {
    switch (obj->type) {
        case STRING:
            builder_add_string(builder, obj->data.str, vstr_len(obj->data.str));
            break;
        case OINT64:
            builder_add_int(builder, obj->data.i64);
            break;
        default:
            printf("CANNOT HANDLE NULL CASE UH OH\n");
            break;
    }
}

