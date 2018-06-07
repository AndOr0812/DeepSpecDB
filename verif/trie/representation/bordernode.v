(** * bordernode_rep.v : Formalization for representation relationship of bordernode *)
Require Import VST.floyd.library.
Require Import VST.msl.iter_sepcon.
Require Import DB.common.

(* functional part *)
Require Import DB.functional.keyslice.
Require Import DB.functional.bordernode.

(* seplogic part *)
Require Import DB.representation.string.

(* program part *)
Require Import DB.prog.

Module BorderNodeValue <: VALUE_TYPE.
  Definition type := val.
  Definition default := nullval.
  Definition inhabitant_value := Vundef.
End BorderNodeValue.

Module BorderNode := BorderNode BorderNodeValue.

Definition tbordernode := Tstruct _BorderNode noattr.

Import BorderNode.

Definition bordernode_rep (sh: share) (s: store) (p: val): mpred :=
  match s with
  | (prefixes, suffix, value) =>
    !! (Forall (fun p => is_pointer_or_null p) (prefixes)) &&
    !! (is_pointer_or_null value) &&
    field_at sh tbordernode [StructField _prefixLinks] prefixes p *
    field_at sh tbordernode [StructField _suffixLink] value p *
    match suffix with
    | Some k =>
      EX p': val,
             field_at sh tbordernode [StructField _keySuffix] p' p *
             field_at sh tbordernode [StructField _keySuffixLength] (Vint (Int.repr (Zlength k))) p *
             cstring_len Tsh k p' *
             malloc_token Tsh (tarray tschar (Zlength k)) p'
    | None =>
      field_at sh tbordernode [StructField _keySuffix] nullval p *
      field_at sh tbordernode [StructField _keySuffixLength] (Vint Int.zero) p
    end
  end.

Theorem bordernoderep_invariant (s: store): forall sh p,
    bordernode_rep sh s p |-- !! invariant s.
Proof.
  intros.
  unfold invariant.
  unfold bordernode_rep.
  destruct s as [[]].
  simpl.
  entailer!.
  destruct H1 as [? _].
  change (Z.max 0 4) with 4 in H1.
  assumption.
Qed.

Hint Resolve bordernoderep_invariant: saturate_local.

Definition tbordernode_fold: forall sh p prefixes v p' len,
  field_at sh tbordernode [StructField _prefixLinks] prefixes p *
  field_at sh tbordernode [StructField _suffixLink] v p *
  field_at sh tbordernode [StructField _keySuffix] p' p *
  field_at sh tbordernode [StructField _keySuffixLength] len p =
           data_at sh tbordernode (prefixes, (v, (p', len))) p.
Proof.
  intros.
  unfold_data_at 1%nat.
  do 2 rewrite <- sepcon_assoc.
  reflexivity.
Qed.

Ltac fold_tbordernode' lemma patterns :=
  match patterns with
  | nil => sep_apply lemma
  | ?hd :: ?tl => match goal with
                 | |- context [field_at _ _ [_ hd] ?t _] =>
                   fold_tbordernode' (lemma t) tl
                 | _ => fail 1 "pattern not found"
                 end
  end.

Ltac fold_tbordernode :=
  match goal with
  | |- context [field_at ?sh tbordernode _ _ ?p] =>
    fold_tbordernode' (tbordernode_fold sh p) [_prefixLinks; _suffixLink; _keySuffix; _keySuffixLength]
  end.
