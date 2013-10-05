
;---------------------------------------------------------------------------
;  init.clp - CLIPS agent initialization file
;
;  Created: Sat Jun 16 12:34:54 2012 (Mexico City)
;  Copyright  2012  Tim Niemueller [www.niemueller.de]
;  Licensed under GPLv2+ license, cf. LICENSE file
;---------------------------------------------------------------------------

(defglobal
  ?*CONFIG_PREFIX* = "/clips-agent"
)


(path-load config.clp)
(path-load skills.clp)


(defrule load-config
  (agent-init)
  =>
  (load-config ?*CONFIG_PREFIX*)
)

(defrule load-agent
  (agent-init)
  (confval (path "/clips-agent/agent") (type STRING) (value ?v))
  =>
  (printout t "Loading agent '" ?v "'" crlf)
  (bind ?agent-file (path-resolve (str-cat ?v ".clp")))
  (if ?agent-file
    then (batch* ?agent-file)
    else (printout logerror "Cannot find agent file " ?v crlf))
)

(defrule enable-debug
  (agent-init)
  (confval (path "/clips-agent/clips-debug") (type BOOL) (value true))
  =>
  (printout t "CLIPS debugging enabled, watching facts and rules" crlf)
  (watch facts)
  (watch rules)
  ;(dribble-on "trace.txt")
)

(defrule debug-level
  (agent-init)
  (confval (path "/clips-agent/debug-level") (type UINT) (value ?v))
  =>
  (printout t "Setting debug level to " ?v " (was " ?*DEBUG* ")" crlf)
  (debug-set-level ?v)
)

(defrule silence-debug-facts
  (declare (salience -1000))
  (agent-init)
  (confval (path "/clips-agent/clips-debug") (type BOOL) (value true))
  (confval (path "/clips-agent/unwatch-facts") (type STRING) (is-list TRUE) (list-value $?lv))
  =>
  (printout t "Disabling watching of the following facts: " ?lv crlf)
  (foreach ?v ?lv (unwatch facts (sym-cat ?v)))
)

(defrule silence-debug-rules
  (declare (salience -1000))
  (agent-init)
  (confval (path "/clips-agent/clips-debug") (type BOOL) (value true))
  (confval (path "/clips-agent/unwatch-rules") (type STRING) (is-list TRUE) (list-value $?lv))
  =>
  (printout t "Disabling watching of the following rules: " ?lv crlf)
  (foreach ?v ?lv (unwatch rules (sym-cat ?v)))
)
