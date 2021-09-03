
#ifndef _oos_h_

#define _oos_h_

#include <stdlib.h>
#include "pl.h"
#include "lib_cairox.h"
#define Boolean short

typedef enum box_type {
    BOX_PROCESS, BOX_BUFFER, BOX_TYPE_MAX
} BoxType;

extern char *oos_class_name[BOX_TYPE_MAX];

typedef enum message_type {
    MT_SEND, MT_ADD, MT_DELETE, MT_EXCITE, MT_INHIBIT, MT_CLEAR, MT_STOP, MT_MAX
} MessageType;

extern char *oos_message_type_name[MT_MAX];

typedef enum buffer_decay_prop {
    BUFFER_DECAY_NONE, BUFFER_DECAY_FIXED, BUFFER_DECAY_LINEAR, BUFFER_DECAY_QUADRATIC, BUFFER_DECAY_EXPONENTIAL, BUFFER_DECAY_WEIBULL
} BufferDecayProp;

typedef enum buffer_capacity_prop {
    BUFFER_CAPACITY_LIMITED, BUFFER_CAPACITY_UNLIMITED
} BufferCapacityProp;

typedef enum buffer_excess_prop {
    BUFFER_EXCESS_IGNORE, BUFFER_EXCESS_RANDOM, BUFFER_EXCESS_OLDEST, BUFFER_EXCESS_YOUNGEST
} BufferExcessProp;

typedef enum buffer_access_prop {
    BUFFER_ACCESS_RANDOM, BUFFER_ACCESS_LIFO, BUFFER_ACCESS_FIFO
} BufferAccessProp;

typedef enum line_style {
    LS_SOLID, LS_DOTTED, LS_DASHED
} LineStyle;

typedef struct timestamped_clause_list {
    ClauseType *head;
    long timestamp;
    double activation;
    struct timestamped_clause_list *tail;
} TimestampedClauseList;

typedef struct message_list {
    int source;
    int target;
    MessageType mt;
    ClauseType *content;
    struct message_list *next;
} MessageList;

typedef struct oos_vars {
    int cycle;
    int block;
    int trials_per_subject;
    int subjects_per_experiment;
    Boolean stopped;
    char *name;
    void *task_data;
    struct box_list *components;
    struct arrow_list *arrows;
    struct annotation_list *annotations;
    struct message_list *messages;
} OosVars;

typedef struct annotation_list {
    char *text;
    double x;
    double y;
    int fontsize;
    double theta;
    Boolean italic;
    struct annotation_list *next;
} AnnotationList;

typedef struct arrow_list {
    VertexStyle vertex_style;
    ArrowHeadType head;
    CairoxPoint *coordinates;
    int points;
    double width;
    LineStyle line_style;
    struct arrow_list *next;
} ArrowList;

typedef struct box_list {
    int id;
    char *name;
    BoxType bt;
    Boolean stopped;
    double x;
    double y;
    void (*output_function)(OosVars *);
    /* Properties (for buffers): */
    BufferDecayProp decay;
    int decay_constant;
    BufferCapacityProp capacity;
    int capacity_constant;
    BufferExcessProp excess_capacity;
    BufferAccessProp access;
    TimestampedClauseList *content;
    struct box_list *next;
} BoxList;

extern void random_initialise(void);
extern int random_integer(int min, int max);
extern double random_uniform(double min, double max);

extern AnnotationList  *oos_annotation_create(OosVars *gv, char *text, double x, double y, int fontsize, double theta, Boolean italic);
extern ArrowList       *oos_arrow_create(OosVars *gv, VertexStyle vs, LineStyle ls, ArrowHeadType head, CairoxPoint *coordinates, int points, double width);
extern BoxList         *oos_process_create(OosVars *gv, char *name, int id, double x, double y, void (*output_function)(OosVars *));
extern BoxList         *oos_buffer_create(OosVars *gv, char *name, int id, double x, double y, BufferDecayProp decay, int decay_constant, BufferCapacityProp capacity, int capacity_constant, BufferExcessProp excess_capacity, BufferAccessProp access);
extern void             oos_buffer_create_element(OosVars *gv, int box_id, char *element, double activation);
extern OosVars         *oos_globals_create();
extern void             oos_messages_free(OosVars *gv);
extern void             oos_model_free(OosVars *gv);
extern void             oos_components_free(OosVars *gv);
extern void             oos_globals_destroy(OosVars *gv);
extern void             oos_dump(OosVars *gv, Boolean state);

extern char *oos_box_name(OosVars *gv, int id);

extern Boolean      oos_match(OosVars *gv, int id, ClauseType *template);
extern Boolean      oos_match_above_threshold(OosVars *gv, int id, ClauseType *template, double threshold);
extern void         oos_message_create(OosVars *gv, MessageType mt, int source, int target, ClauseType *content);

extern Boolean      oos_step(OosVars *gv);
extern void         oos_step_block(OosVars *gv);
extern void         oos_initialise_trial(OosVars *gv);
extern void         oos_initialise_session(OosVars *gv, int trials_per_subject, int subjects_per_experiment);

extern TimestampedClauseList  *oos_buffer_get_contents(OosVars *gv, int id);

#endif
