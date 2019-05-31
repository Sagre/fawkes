(defrule diagnosis-create-plan-n-goal
    (plan-action (goal-id ?goal-id) (plan-id ?plan-id))
    (not (goal (id ?goal-id)))
    (not (plan (id ?plan-id)))
    =>
    (assert
        (goal (id ?goal-id) (mode DISPATCHED))
        (plan (id ?plan-id) (goal-id ?goal-id))
    )

)

(defrule diagnosis-execute-plan-actions
    (declare (salience -1))
    (diagnosis-setup-finished)
    ?dss <- (diagnosis-setup-stage (state DOMAIN-LOADED))
    ?pa <- (plan-action (id ?id) (state FORMULATED) (action-name ?an)); (executable TRUE))
    (not (plan-action (id ?id2&:(< ?id2 ?id)) (state FORMULATED)))
    (not (plan-action (goal-id ?goal-id) (plan-id ?plan-id) (state ?state&:(and (neq ?state FINAL) (neq ?state FORMULATED)))))
    =>
    (modify ?pa (state EXECUTION-SUCCEEDED))
    (printout t "Seleceted next action " ?an crlf)
)

(defrule diagnosis-plan-action-not-executable
    (declare (salience -1))
    (diagnosis-setup-finished)
    ?dss <- (diagnosis-setup-stage (state DOMAIN-LOADED))
    ?pa <- (plan-action (id ?id) (state FORMULATED) (action-name ?an) (executable FALSE))
    (not (plan-action (id ?id2&:(< ?id2 ?id)) (state FORMULATED)))
    =>
    (printout error "Next plan action " ?an " is not executable" crlf)
)

(defrule diagnosis-history-propagation-finished
    (not (plan-action (state ~FINAL)))
    ?dss <- (diagnosis-setup-stage (state DOMAIN-LOADED))
    (plan-action (state FINAL))
    =>
    (modify ?dss (state HISTORY-PROPAGATED))
    (printout t "Finished history propagation" crlf)
)