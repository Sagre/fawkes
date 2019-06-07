(deftemplate diagnosis-hypothesis
    (slot id (type SYMBOL))
    (slot state (type SYMBOL) (allowed-values INIT WM-FACTS-INIT ACTIONS-PROPAGATED FAILED))
    (slot probability (type FLOAT) (default -1.0))
)

(deftemplate diagnosis-setup-stage
	(slot state (type SYMBOL) (allowed-values INIT DOMAIN-LOADED HISTORY-PROPAGATED FAILED))
)

(deftemplate diagnosis-sensing-result
    (slot predicate (type SYMBOL))
    (multislot args)
    (slot value (type SYMBOL))
)

(deffunction diagnosis-exclude-hypothesis (?diag-id)
    (printout t "Exclude diagnosis hypothesis " ?diag-id crlf)
    (do-for-all-facts ((?wm wm-fact)) (eq ?wm:env ?diag-id)
        (retract ?wm)
    )
    (do-for-all-facts ((?dh diagnosis-hypothesis)) (eq ?dh:id ?diag-id)
        (do-for-all-facts ((?pa plan-action)) (eq ?pa:diag-id ?dh:id)
            (retract ?pa)
        )
        (retract ?dh)
    )
)

(defrule diagnosis-start-initialize
    (active-diagnosis-initialized)
    (not (diagnosis-setup-stage))
    =>
    (assert (diagnosis-setup-stage (state INIT)))
)

(defrule diagnosis-domain-load
  ?dss <- (diagnosis-setup-stage (state INIT))
  =>
  (parse-pddl-domain (path-resolve "rcll2018/domain.pddl"))
  (assert (domain-loaded))
  (modify ?dss (state DOMAIN-LOADED))
  (printout t "Finished parsing pddl domain" crlf)
)

(defrule diagnosis-copy-wm-facts
    ?dh <- (diagnosis-hypothesis (id ?id) (state INIT))
    =>
    (do-for-all-facts ((?wm wm-fact)) (and (wm-key-prefix ?wm:key (create$ domain fact)) (eq ?wm:env DEFAULT))
        (duplicate ?wm (env ?id))
    )
    (printout t "Copy wm-facts for " ?id crlf)
    (modify ?dh (state WM-FACTS-INIT))
)

(defrule diagnosis-calculate-probability
    ?dh <- (diagnosis-hypothesis (id ?id) (probability -1.0))
    (diagnosis-setup-finished)
    =>
    (bind ?prob 1.0)
    (do-for-all-facts ((?pa plan-action)) (eq ?pa:diag-id ?id)
        (do-for-fact ((?wm wm-fact)) (and (wm-key-prefix ?wm:key (create$ hardware edge)) 
                                          (eq ?pa:action-name (wm-key-arg ?wm:key trans))
                                     )
            (bind ?prob (* ?prob (string-to-field (str-cat (wm-key-arg ?wm:key prob)))))
        )
    )
    (modify ?dh (probability ?prob))
)

(defrule diagnosis-execute-plan-actions
    (declare (salience -1))
    (diagnosis-setup-finished)
    ?dss <- (diagnosis-setup-stage (state DOMAIN-LOADED))
    (diagnosis-hypothesis (id ?diag-id) (state WM-FACTS-INIT))
    ?pa <- (plan-action (diag-id ?diag-id) (id ?id) (state FORMULATED) (action-name ?an) (executable TRUE))
    (not (plan-action (diag-id ?diag-id) (id ?id2&:(< ?id2 ?id)) (state FORMULATED)))
    (not (plan-action (diag-id ?diag-id) (state ?state&:(and (neq ?state FINAL) (neq ?state FORMULATED)))))
    =>
    (modify ?pa (state EXECUTION-SUCCEEDED))
    (printout t "Seleceted next action " ?an " for " ?diag-id crlf)
)

(defrule diagnosis-plan-action-not-executable
    (declare (salience -1))
    (diagnosis-setup-finished)
    ?dss <- (diagnosis-setup-stage (state DOMAIN-LOADED))    
    ?dh <- (diagnosis-hypothesis (id ?diag-id) (state WM-FACTS-INIT))
    ?pa <- (plan-action (id ?id) (diag-id ?diag-id) (state FORMULATED) (action-name ?an) (executable FALSE))
    (not (plan-action (diag-id ?diag-id) (id ?id2&:(< ?id2 ?id)) (state FORMULATED)))
    =>
    (printout error "Next plan action " ?an " of " ?diag-id " is not executable" crlf)
    (modify ?dh (state FAILED))
)

(defrule diagnosis-hypothesis-done
    (diagnosis-setup-finished)
    ?dh <- (diagnosis-hypothesis (id ?diag-id) (state WM-FACTS-INIT))
    (not (plan-action (diag-id ?diag-id) (state ?s&:(neq ?s FINAL))))
    =>
    (printout t "Finished history propagation " ?diag-id crlf)
    (modify ?dh (state ACTIONS-PROPAGATED))
)

(defrule diagnosis-history-propagation-finished
    (diagnosis-setup-finished)
    (not (diagnosis-hypothesis (state ?s&:(and (neq ?s ACTIONS-PROPAGATED) (neq ?s FAILED)))))
    ?dss <- (diagnosis-setup-stage (state DOMAIN-LOADED))
    =>
    (modify ?dss (state HISTORY-PROPAGATED))
)

(defrule diagnosis-exclude-hypothesis-positive
    ?dsr <- (diagnosis-sensing-result (predicate ?pred)
                    (args $?pred-args)
                    (value TRUE))
    (diagnosis-hypothesis (id ?env))
    (or (wm-fact (key domain fact ?pred args? $?pred-args) (env ?env) (value FALSE))
        (not (wm-fact (key domain fact ?pred args? $?pred-args) (env ?env)))
    )
    =>
    (diagnosis-exclude-hypothesis ?env)
)

(defrule diagnosis-exclude-hypothesis-negative
    ?dsr <- (diagnosis-sensing-result (predicate ?pred)
                    (args $?pred-args)
                    (value FALSE))
    (diagnosis-hypothesis (id ?env))
    (wm-fact (key domain fact ?pred args? $?pred-args) (env ?env) (value ~FALSE))
    =>
    (diagnosis-exclude-hypothesis ?env)
)

(defrule diagnosis-sensing-result-cleanup
    (declare (salience -1))
    ?dsr <- (diagnosis-sensing-result (predicate ?pred) (args $?pred-args) (value ?val))
    =>
    (printout t "Finished integrating sensing result ." ?pred ". " ?pred-args " " ?val crlf)
  ;  (retract ?dsr)
)
