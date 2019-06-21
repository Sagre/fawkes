
;---------------------------------------------------------------------------
;  plan.clp - CLIPS executive - plan representation
;
;  Created: Wed Sep 20 15:47:23 2017
;  Copyright  2017  Tim Niemueller [www.niemueller.de]
;  Licensed under GPLv2+ license, cf. LICENSE file
;---------------------------------------------------------------------------

(deftemplate plan-action
	(slot id (type INTEGER))
	(slot diag-id (type SYMBOL))
	(slot action-name (type SYMBOL))
	(multislot param-names)
	(multislot param-values)
	(slot duration (type FLOAT))
	(slot dispatch-time (type FLOAT) (default -1.0))
	(slot state (type SYMBOL)
	            (allowed-values FORMULATED PENDING WAITING RUNNING EXECUTION-SUCCEEDED
	                            SENSED-EFFECTS-WAIT SENSED-EFFECTS-HOLD EFFECTS-APPLIED
	                            FINAL EXECUTION-FAILED FAILED))
	(slot executable (type SYMBOL) (allowed-values TRUE FALSE) (default FALSE))
	(slot error-msg (type STRING))
)

(deffunction plan-action-arg (?param-name ?param-names ?param-values $?default)
	(foreach ?p ?param-names
		(if (eq ?param-name ?p) then (return (nth$ ?p-index ?param-values)))
	)
	(if (> (length$ ?default) 0) then (return (nth$ 1 ?default)))
	(return FALSE)
)