(** * tactics.v : General Tactics *)
Require Import VST.floyd.proofauto.
Require Export Coq.Program.Tactics.

Tactic Notation "if_tac" := 
  match goal with |- context [if ?a then _ else _] =>
    lazymatch type of a with
    | sumbool _ _ => destruct a as [?H | ?H]
    | option _ => destruct a eqn:?H
    | bool => fail "Use simple_if_tac instead of if_tac, since your expression"a" has type bool"
    | ?t => fail "Use if_tac only for sumbool; your expression"a" has type" t
   end end.

Tactic Notation "if_tac" simple_intropattern(H)
   := match goal with |- context [if ?a then _ else _] =>
    lazymatch type of a with
    | sumbool _ _ => destruct a as H
    | option _ => destruct a eqn:H
    | bool => fail "Use simple_if_tac instead of if_tac, since your expression"a" has type bool"
    | ?t => fail "Use if_tac only for sumbool; your expression"a" has type" t
   end end.

Tactic Notation "if_tac" "in" hyp(H0)
 := match type of H0 with context [if ?a then _ else _] =>
    lazymatch type of a with
    | sumbool _ _ => destruct a as [?H | ?H]
    | option _ => destruct a eqn:?H
    | bool => fail "Use simple_if_tac instead of if_tac, since your expression"a" has type bool"
    | ?t => fail "Use if_tac only for sumbool; your expression"a" has type" t
   end end.

Tactic Notation "if_tac" simple_intropattern(H) "in" hyp(H1)
 := match type of H1 with context [if ?a then _ else _] => 
    lazymatch type of a with
    | sumbool _ _ => destruct a as H
    | option _ => destruct a eqn:H
    | bool => fail "Use simple_if_tac instead of if_tac, since your expression"a" has type bool"
    | ?t => fail "Use if_tac only for sumbool; your expression"a" has type" t
   end end.

Tactic Notation "match_tac" := 
  match goal with |- context [match ?a with _ => _ end] =>
                  destruct a eqn:?H
  end.

Tactic Notation "match_tac" simple_intropattern(H) :=
  match goal with |- context [match ?a with _ => _ end] =>
                      destruct a eqn:H
  end.

Tactic Notation "match_tac" "in" hyp(H0) :=
  match type of H0 with context [match ?a with _ => _ end] =>
                        destruct a eqn:?H
  end.

Tactic Notation "match_tac" simple_intropattern(H) "in" hyp(H1) :=
  match type of H1 with context [match ?a with _ => _ end] =>
                        destruct a eqn:H
  end.


Ltac elim_cast_pointer' term t :=
  match t with
  | force_val (sem_cast_pointer ?t') => elim_cast_pointer' term t'
  | _ =>
    let H := fresh "H" in
    first [
        apply (assert_later_PROP (is_pointer_or_null t))
      | apply (assert_PROP (is_pointer_or_null t))
      | apply (assert_PROP' (is_pointer_or_null t))];
    [ now entailer!
    | intro H ];
    idtac term;
    idtac t;
    replace term with t by (destruct t; first [contradiction | reflexivity]);
    clear H
  end.

Ltac elim_cast_pointer :=
  match goal with
  | |- context [force_val (sem_cast_pointer ?t)] =>
    elim_cast_pointer' (force_val (sem_cast_pointer t)) t
  end.

Ltac simplify :=
      destruct_conjs;
      destruct_exists;
      repeat (match goal with
              | var: _ * _ |- _ => destruct var
              | H: (_, _) = (_, _) |- _ => inv H
              | H: ?f _ = ?f _ |- _ => injection H; intros; subst; clear H
              | H: ?f _ _ = ?f _ _ |- _ => injection H; intros; subst; clear H
              | H: _ /\ _ |- _ => destruct_conjs
              | |- _ /\ _ => split
              | H: fst ?p = _ |- _ => destruct p; simpl in H; subst
              | H: snd ?p = _ |- _ => destruct p; simpl in H; subst
              | H: _ = fst ?p |- _ => destruct p; simpl in H; subst
              | H: _ = snd ?p |- _ => destruct p; simpl in H; subst
              | H: match _ with _ => _ end |- _ => match_tac in H; subst
              | H: match _ with _ => _ end = _ |- _ => match_tac in H; subst
              | H: _ = match _ with _ => _ end |- _ => match_tac in H; subst
              | H: False |- _ => destruct H
              end);
      eauto;
      try discriminates.
              