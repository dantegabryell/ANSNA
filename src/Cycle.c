#include "Cycle.h"

int currentTime = 0;
void composition(Concept *B, Concept *A, Event *b)
{
    //temporal induction and intersection
    if(A->event_beliefs.itemsAmount > 0)
    {
        Event *a = &A->event_beliefs.array[0]; //most recent, highest revised
        if(a->type != EVENT_TYPE_DELETED && !Stamp_checkOverlap(&a->stamp, &b->stamp))
        {
            Implication result = Inference_BeliefInduction(&a, &b);
            Table_AddAndRevise(&B->precondition_beliefs, &result);
            Table_AddAndRevise(&A->postcondition_beliefs, &result);
        }
    }
}

void decomposition(Concept *c, Event *e)
{
    //detachment
    if(e->type == EVENT_TYPE_BELIEF)
    {
        if(c->postcondition_beliefs.itemsAmount>0)
        {
            Implication postcon = c->postcondition_beliefs.array[0];
            if(!Stamp_checkOverlap(&e->stamp, &postcon.stamp))
            {
                Event res = Inference_BeliefDeduction(&e, &postcon);
                res.attention = Attention_deriveEvent(&c->attention, &postcon.truth);
                c->postcondition_beliefs.array[0] = Inference_AssumptionOfFailure(&postcon); //TODO do better
                PriorityQueue_Push_Feedback pushed = PriorityQueue_Push(&events, res.attention.priority);
                Event *toRecyle = pushed.addedItem.address;
                *toRecyle = res;
            }
        }
        if(c->precondition_beliefs.itemsAmount>0)
        {
            Implication precon = c->precondition_beliefs.array[0];
            if(!Stamp_checkOverlap(&e->stamp, &precon.stamp))
            {
                Event res = Inference_BeliefAbduction(&e, &precon);
                res.attention = Attention_deriveEvent(&c->attention, &precon.truth);
                PriorityQueue_Push_Feedback pushed = PriorityQueue_Push(&events, res.attention.priority);
                Event *toRecyle = pushed.addedItem.address;
                *toRecyle = res;
            }
        }
    }
    else
    if(e->type == EVENT_TYPE_GOAL)
    {
        if(c->postcondition_beliefs.itemsAmount>0)
        {
            Implication postcon = c->postcondition_beliefs.array[0];
            if(!Stamp_checkOverlap(&e->stamp, &postcon.stamp))
            {
                Event res = Inference_GoalAbduction(&e, &postcon);
                res.attention = Attention_deriveEvent(&c->attention, &postcon.truth);
                PriorityQueue_Push_Feedback pushed = PriorityQueue_Push(&events, res.attention.priority);
                Event *toRecyle = pushed.addedItem.address;
                *toRecyle = res;
            }
        }
        if(c->precondition_beliefs.itemsAmount>0)
        {
            Implication precon = c->precondition_beliefs.array[0];
            if(!Stamp_checkOverlap(&e->stamp, &precon.stamp))
            {
                Event res = Inference_GoalDeduction(&e, &precon);
                res.attention = Attention_deriveEvent(&c->attention, &precon.truth);
                PriorityQueue_Push_Feedback pushed = PriorityQueue_Push(&events, res.attention.priority);
                Event *toRecyle = pushed.addedItem.address;
                *toRecyle = res;
            }
        }
    }
}

void cycle()
{
    for(int i=0; i<EVENT_SELECTIONS; i++)
    {
        //1. get an event from the event queue
        Item item = PriorityQueue_PopMax(&events);
        Event *e = item.address;
        //determine the concept it is related to
        int closest_concept_i = Memory_getClosestConcept(&e);
        if(closest_concept_i == MEMORY_MATCH_NO_CONCEPT)
        {
            continue;
        }
        Concept *c = concepts.items[closest_concept_i].address;
        //apply decomposition-based inference: prediction/explanation
        decomposition(c, e);
        //add event to the FIFO of the concept
        FIFO *fifo =  e->type == EVENT_TYPE_BELIEF ? &c->event_beliefs : &c->event_goals;
        Event revised = FIFO_AddAndRevise(e, fifo);
        if(revised.type != EVENT_TYPE_DELETED)
        {
            PriorityQueue_Push_Feedback pushed = PriorityQueue_Push(&events, revised.attention.priority);
            Event *toRecycle = pushed.addedItem.address;
            *toRecycle = revised;
        }
        //relatively forget the event, as it was used, and add back to events
        e->attention = Attention_forgetEvent(&e->attention);
        PriorityQueue_Push_Feedback pushed = PriorityQueue_Push(&events, e->attention.priority);
        Event *toRecyle = pushed.addedItem.address;
        *toRecyle = *e;
        //trigger composition-based inference hypothesis formation
        for(int j=0; j<CONCEPT_SELECTIONS; j++)
        {
            Item item = PriorityQueue_PopMax(&concepts);
            Concept *d = item.address;
            composition(c, d, e); // deriving a =/> b
        }
        //activate concepts attention with the event's attention
        c->attention = Attention_activateConcept(&c->attention, &e->attention); 
        PriorityQueue_bubbleUp(&concepts,closest_concept_i); //priority was increased
        //add a new concept for e too at the end, just before it needs to be identified with something existing
        Memory_addConcept(&e->sdr, Attention_activateConcept(&c->attention, &e->attention));
    }
    //relative forget concepts:
    for(int i=0; i<concepts.itemsAmount; i++) //as all concepts are forgotten the order won't change
    { //making this operation very cheap, not demanding any heap operation
        Concept *c = concepts.items[i].address;
        c->attention = Attention_forgetConcept(&c->attention, &c->usage, currentTime);
        concepts.items[i].priority = c->attention.priority;
    }
    currentTime++;
}