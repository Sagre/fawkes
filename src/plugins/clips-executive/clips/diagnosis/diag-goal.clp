(deftemplate diag-plan-information-gain
    (slot plan-id (type SYMBOL))
    (slot gain (type FLOAT))
)

(deffunction diag-clean-up-hypotheses (?diag-id)
    (do-for-all-facts ((?dh diagnosis-hypothesis)) (eq ?dh:diag-id ?diag-id)
        (do-for-all-facts ((?pa plan-action)) (eq ?pa:plan-id ?dh:id)
            (retract ?pa)
        )
        (retract ?dh)
    )
)

(defrule diag-goal-start-active-diagnosis 
    ?d <- (diagnosis (id ?diag-id) (mode DIAGNOSIS-CREATED))
    (not (goal (class ACTIVE-DIAGNOSIS) (params ?diag-id)))
    =>
    (printout t "Started Diagnosis Main Goal" crlf)
    (bind ?setup-ret (active-diagnosis-set-up (str-cat ?diag-id)))
    (if ?setup-ret then
        (assert (goal (class ACTIVE-DIAGNOSIS) (id (sym-cat ACTIVE-DIAGNOSIS- (gensym*)))
                    (params ?diag-id)
                    (sub-type RUN-ALL-OF-SUBGOALS)))
        (diag-clean-up-hypotheses ?diag-id)
        (modify ?d (mode ACTIVE-DIAGNOSIS))
    else
        (printout error "Failed to setup diagnosis environment" crlf)
        (modify ?d (mode FAILED))
    )
    
)

(defrule diag-goal-select-diagnosis-goal
    ?g <- (goal (class ACTIVE-DIAGNOSIS) (mode FORMULATED) (parent nil))
    =>
    (printout t "Selected Diagnosis Main Goal" crlf)
    (modify ?g (mode SELECTED))
)

(defrule diag-goal-expand-diagnosis-goal
    ?g <- (goal (class ACTIVE-DIAGNOSIS) (id ?id) (mode SELECTED) (parent nil))
    =>
    (active-diagnosis-update-common-knowledge)
    (assert 
        (goal (id (sym-cat ACTIVE-DIAGNOSIS-SENSE- (gensym*)))
                (class ACTIVE-DIAGNOSIS-SENSE)
                (parent ?id)
                (type ACHIEVE)
                (mode FORMULATED)
        )
    )
    (modify ?g (mode EXPANDED))
)


(defrule diag-goal-expand-diagnosis-goal-sensing
    (declare (salience ?*SALIENCE-LOW*))
    ?g <- (goal (class ACTIVE-DIAGNOSIS-SENSE) (id ?id) (mode SELECTED))
    (plan (id ?plan-id) (goal-id ?id))
    =>
    (modify ?g (mode EXPANDED) (committed-to ?id))
    (do-for-all-facts ((?p plan)) (eq ?p:goal-id ?id)
        (bind ?sensed-predicate (create$ ))
        (bind ?sensed-predicate-names (create$ ))
        (bind ?sensed-predicate-values (create$ ))
        (do-for-fact ((?pa plan-action)) (and (eq ?pa:plan-id ?p:id) (eq ?pa:executable TRUE))
            (do-for-fact ((?dsa domain-sensing-action)) (eq ?dsa:operator ?pa:action-name)
                (bind ?sensed-predicate (create$ ?sensed-predicate ?dsa:sensed-predicate))
                (foreach ?pn ?dsa:sensed-param-names
                    (bind ?param-index (member$ ?pn ?pa:param-names))
                    (if ?param-index then
                        (bind ?sensed-predicate (create$ ?sensed-predicate ?pn (nth$ ?param-index ?pa:param-values)))
                        (bind ?sensed-predicate-names (create$ ?sensed-predicate-names ?pn))
                        (bind ?sensed-prediacte-values (create$ ?sensed-predicate-values (nth$ ?param-index ?pa:param-values)))
                    else
                        (bind ?param-index (member$ ?pn ?dsa:sensed-param-names))
                        (bind ?const (nth$ ?param-index ?dsa:sensed-constants))
                        (if (neq ?const nil) then
                            (bind ?sensed-predicate (create$ ?sensed-predicate ?pn ?const))
                            (bind ?sensed-predicate-names (create$ ?sensed-predicate-names ?pn))
                            (bind ?sensed-predicate-values (create$ ?sensed-predicate-values ?const))
                        else
                            (printout error "Predicate param name " ?pn " is not set by grounded action and not a constant" crlf)
                        )
                    )
                )
                (assert 
                    (diag-plan-information-gain (plan-id ?p:id) (gain (active-diagnosis-information-gain (implode$ ?sensed-predicate))))
                    (domain-sensing-action-result (plan-id ?p:id) (goal-id ?id) (plan-action-id ?pa:id) 
                                    (predicate ?dsa:sensed-predicate)
                                    (predicate-names ?sensed-predicate-names)
                                    (predicate-values ?sensed-predicate-values)
                                    (state PENDING))
                )
            )
        )

    )
)


(defrule diag-goal-commit-to-plan
    ?g <- (goal (id ?id) (class ACTIVE-DIAGNOSIS-SENSE) (mode EXPANDED))
    (plan (id ?plan-id) (goal-id ?id))
    (diag-plan-information-gain (plan-id ?plan-id) (gain ?gain&:(> ?gain 0.00001)))
    (not (diag-plan-information-gain (gain ?g2&:(> ?g2 ?gain))))
    =>
    (modify ?g (mode COMMITTED) (committed-to ?plan-id))
    (do-for-all-facts ((?p plan)) (and (eq ?p:goal-id ?id) (neq ?p:id ?plan-id))
        (do-for-all-facts ((?pa plan-action)) (eq ?pa:plan-id ?p:id)
            (retract ?pa)
        )
        (do-for-all-facts ((?dpa diag-plan-information-gain)) (eq ?dpa:plan-id ?p:id)
            (retract ?dpa)
        )
        (retract ?p)
    )
)

(defrule diag-goal-no-plan
    ?g <- (goal (id ?id) (class ACTIVE-DIAGNOSIS-SENSE) (mode EXPANDED))
    (or (not (plan (goal-id ?id)))
        (not (and (plan (goal-id ?id) (id ?plan-id))
             (not (diag-plan-information-gain (plan-id ?plan-id) (gain ?gain&:(> ?gain 0.0001)))))
        )
    )
    =>
    (modify ?g (mode RETRACTED) (outcome REJECTED))
)

(defrule diag-goal-dispatch
    ?g <- (goal (class ACTIVE-DIAGNOSIS-SENSE) (mode COMMITTED))
    =>
    (modify ?g (mode DISPATCHED))
)

(defrule diag-goal-evaluate-subgoal
    ?sg <- (goal (id ?id) (class ACTIVE-DIAGNOSIS-SENSE) (parent ?parent-id&~nil)
                (mode FINISHED)
                (outcome ?outcome))
    ?p <- (plan (id ?plan-id) (goal-id ?id))
    ?dpi <- (diag-plan-information-gain (plan-id ?plan-id))
    (plan-action (id ?pa-id) (goal-id ?id) (plan-id ?plan-id) (state FINAL))
    ?dsar <- (domain-sensing-action-result (plan-id ?plan-id) (goal-id ?id) (plan-action-id ?pa-id)
                    (predicate ?predicate) 
                    (predicate-names $?predicate-names) 
                    (predicate-values $?predicate-values))
    ?d <- (diagnosis (id ?gen-id))
    =>
    (printout t "Integrating " ?predicate " " ?predicate-names " " ?predicate-values crlf)
    (bind ?diag-count (active-diagnosis-integrate-measurement 1 (str-cat ?predicate) ?predicate-names ?predicate-values))
    (active-diagnosis-update-common-knowledge)
    (if (and (neq ?diag-count 1) (eq ?outcome COMPLETED)) then
        (assert (goal (id (sym-cat ACTIVE-DIAGNOSIS-SENSE- (gensym*)))
                        (class ACTIVE-DIAGNOSIS-SENSE)
                        (parent ?parent-id)
                        (type ACHIEVE)
                        (mode FORMULATED))  
        )
    else
        (active-diagnosis-delete)
    )
    (retract ?p ?dpi ?dsar)
    (modify ?sg (mode EVALUATED))
)

(defrule diag-goal-evaluate-diag-goal
    ?g <- (goal (id ?parent-id) (class ACTIVE-DIAGNOSIS) (mode FINISHED) (outcome ?outcome) (params ?diag-id))
    ?d <- (diagnosis (id ?diag-id) (mode ACTIVE-DIAGNOSIS))
    =>

    (if (eq ?outcome COMPLETED) then
        (printout t "Diagnosis of " ?diag-id " finished" crlf)
        (modify ?d (mode FINAL))
    else
        (printout t "Diagnosis of " ?diag-id " failed" crlf)
        (modify ?d (mode FAILED))
    )
    (modify ?g (mode EVALUATED))
)

(defrule diag-goal-retract-diag-goal
    ?g <- (goal (id ?parent-id) (class ACTIVE-DIAGNOSIS) (mode EVALUATED))
    =>
    (printout t "Retract diagnosis goal" crlf)
    (modify ?g (mode RETRACTED))
)

(defrule diag-goal-clean-up
    ?g <- (goal (id ?parent-id) (class ACTIVE-DIAGNOSIS) (mode RETRACTED))
    =>
    (printout t "Clean Up Diagnosis Goals" crlf)
    (do-for-all-facts ((?sg goal)) (eq ?sg:parent ?parent-id)
        (do-for-all-facts ((?p plan)) (eq ?p:goal-id ?sg:id)
            (retract ?p)
        )
        (do-for-all-facts ((?pa plan-action)) (eq ?pa:goal-id ?sg:id)
            (retract ?pa)
        )
        (retract ?sg)
    )
    (retract ?g)
    (printout t "Done" crlf)
)

(defrule diag-goal-expand-sense-goal
    ?g <- (goal (id ?id) (class ACTIVE-DIAGNOSIS-SENSE) (mode SELECTED) (params ?action))
    =>
    (assert 
        (plan (id SENSING-PLAN) (goal-id ?id))
        (plan-action (id 1) (plan-id SENSING-PLAN) (goal-id ?id) (action-name (sym-cat ?action)))
    )
    (modify ?g (mode DISPATCHED))
)

(defrule test-action
    ?pa <- (plan-action (action-name test-action) (state PENDING))
    =>
    (modify ?pa (state FINAL))
)