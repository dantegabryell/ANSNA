#include "RuleTable.h"

void RuleTable_Composition(Concept *A, Concept *B, Event *b, long currentTime)
{
    //temporal induction and intersection
    if(b->type == EVENT_TYPE_BELIEF && A->event_beliefs.itemsAmount > 0)
    {
        int k=0;
        for(int i=0;i<A->event_beliefs.itemsAmount; i++)
        {
            Event *a = &A->event_beliefs.array[i];
            if(!Stamp_checkOverlap(&a->stamp, &b->stamp)) //TODO temporal overlap
            {
                Implication precondition_implication =   b->occurrenceTime > a->occurrenceTime ? Inference_BeliefInduction(a, b, false) : Inference_BeliefInduction(b, a, false);
                Implication postcondition_implication =  b->occurrenceTime > a->occurrenceTime ? Inference_BeliefInduction(a, b, true)  : Inference_BeliefInduction(b, a, true);
                if(k>MAX_INDUCTIONS)
                {
                    break;
                }
                k++;
                if(precondition_implication.truth.confidence >= MIN_CONFIDENCE) //has same truth as postcon, just different SDR
                {
                    IN_OUTPUT( printf("Formed (pre- and post-condition) implication: "); Implication_Print(&postcondition_implication); Implication_Print(&precondition_implication); )
                    Implication revised_precon = Table_AddAndRevise(&B->precondition_beliefs, &precondition_implication);
                    IN_OUTPUT( if(revised_precon.sdr_hash != 0) { printf("REVISED pre-condition implication: "); Implication_Print(&revised_precon); } )
                    Implication revised_postcon = Table_AddAndRevise(&A->postcondition_beliefs, &postcondition_implication);
                    IN_OUTPUT( if(revised_postcon.sdr_hash != 0) { printf("REVISED post-condition implication: "); Implication_Print(&revised_postcon); } )
                }
                Event sequence = b->occurrenceTime > a->occurrenceTime ? Inference_BeliefIntersection(a, b) : Inference_BeliefIntersection(b, a);
                sequence.attention = Attention_deriveEvent(&B->attention, &a->truth, currentTime);
                if(sequence.truth.confidence < MIN_CONFIDENCE || sequence.attention.priority < MIN_PRIORITY)
                {
                    continue;
                }
                derivations[eventsDerived++] = sequence;
                IN_OUTPUT( printf("COMPOSED SEQUENCE EVENT: "); Event_Print(&sequence); )
            }
        }
    }
}

void RuleTable_Decomposition(Concept *c, Event *e, long currentTime)
{
    //detachment
    if(c->postcondition_beliefs.itemsAmount>0)
    {
        int k=0;
        for(int i=0; i<c->postcondition_beliefs.itemsAmount; i++)
        {
            Implication postcon = c->postcondition_beliefs.array[i];
            if(!Stamp_checkOverlap(&e->stamp, &postcon.stamp))
            {
                Event res = e->type == EVENT_TYPE_BELIEF ? Inference_BeliefDeduction(e, &postcon) : Inference_GoalAbduction(e, &postcon);
                if(res.type == EVENT_TYPE_GOAL && !ALLOW_ABDUCTION)
                {
                    continue;
                }
                res.attention = Attention_deriveEvent(&c->attention, &postcon.truth, currentTime);
                if(res.truth.confidence < MIN_CONFIDENCE || res.attention.priority < MIN_PRIORITY)
                {
                    continue;
                }
                derivations[eventsDerived++] = res;
                //add negative evidence to the used predictive hypothesis (assumption of failure, for extinction)
                Table_PopHighestTruthExpectationElement(&c->postcondition_beliefs);
                Implication updated = Inference_AssumptionOfFailure(&postcon);
                Table_Add(&c->postcondition_beliefs, &updated);
                k++;
                IN_OUTPUT( printf("DETACHED FORWARD EVENT: "); Event_Print(&res); )
                if(k>MAX_FORWARD)
                {
                    break;
                }
            }
        }
    }
    if(c->precondition_beliefs.itemsAmount>0)
    {
        int k=0;
        for(int i=0; i<c->precondition_beliefs.itemsAmount; i++)
        {
            Implication precon = c->precondition_beliefs.array[i];
            if(!Stamp_checkOverlap(&e->stamp, &precon.stamp))
            {
                Event res = e->type == EVENT_TYPE_BELIEF ? Inference_BeliefAbduction(e, &precon) : Inference_GoalDeduction(e, &precon);
                if(res.type == EVENT_TYPE_BELIEF && !ALLOW_ABDUCTION)
                {
                    continue;
                }
                res.attention = Attention_deriveEvent(&c->attention, &precon.truth, currentTime);
                if(res.truth.confidence < MIN_CONFIDENCE || res.attention.priority < MIN_PRIORITY)
                {
                    continue;
                }
                derivations[eventsDerived++] = res;
                k++;
                IN_OUTPUT( printf("DETACHED BACKWARD EVENT: "); Event_Print(&res); )
                if(k>MAX_BACKWARD)
                {
                    break;
                }
            }
        }
    }
}
