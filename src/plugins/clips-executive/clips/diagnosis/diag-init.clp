(deftemplate diag-interface-pending
  (slot id (type STRING))
  (slot if-type (type STRING))
)

(defrule diagnosis-init-open-interfaces
  " Open blackboard interfaces to interact with diagnosis creation "
  (executive-init)
  (ff-feature-loaded blackboard)
  =>
  (blackboard-open-reading "PddlDiagInterface" "diag-gen")
  (assert (diag-interface-pending (id "diag-gen") (if-type "PddlDiagInterface")))
)

(defrule diagnosis-init-if-writer
  ?bi <- (blackboard-interface-info (id "diag-gen") (has-writer TRUE))
  ?di <- (diag-interface-pending (id "diag-gen"))
  =>
  (retract ?bi)
  (retract ?di)
  (path-load "diagnosis/diag-pddl.clp")
  (path-load "diagnosis/diag-wm.clp")
  (path-load "diagnosis/diag-goal.clp")
)
