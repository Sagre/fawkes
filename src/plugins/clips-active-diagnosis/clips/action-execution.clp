(defrule action-execution
  ?pa <- (plan-action (state PENDING) (executable TRUE))
  =>
  (modify ?pa (state EXECUTION-SUCCEEDED))
)
