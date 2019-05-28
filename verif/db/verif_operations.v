Require Import VST.floyd.proofauto.

Require Import Signatures.

Require Import db_operations.

Instance CompSpecs : compspecs. make_compspecs prog. Defined.
Definition Vprog : varspecs. mk_varspecs prog. Defined.

Require Import FiniteSet FlatData SortedTuples SortedAttributes Values Plans.

Fixpoint string_to_list_byte (s: string) : list byte :=
  match s with
  | EmptyString => nil
  | String a s' => Byte.repr (Z.of_N (Ascii.N_of_ascii a)) :: string_to_list_byte s'
  end.

Definition stringpool : Type := (* This is only temporary. *)
  { v | (type_of_value v) = type_string } -> val.

Program Definition val_of_value (sp : stringpool) (v : value) : val :=
  match v with
  | Value_string s => sp (exist _ v _)
  | Value_ptrofs x => Vint (Ptrofs.to_int x) end.

(* This need to be implemented. *)
Definition stringpool_rep (sp : stringpool) : mpred := emp.

Definition tuple := Tuple1.tuple attribute value OAN FAN.
Definition support := Tuple1.support attribute value OAN FAN.
Definition dot := Tuple1.dot attribute value OAN FAN.

Definition tuple_rep (sh : share) (t : tuple) (p : val) : mpred :=
  EX sp : stringpool,
          data_at sh (tarray size_t (Z.of_nat (Fset.cardinal FAN (support t))))
                  (map (val_of_value sp oo dot t) (Fset.elements FAN (support t))) p * stringpool_rep sp.

Definition t_attr_list := Tstruct _attribute_list_t noattr.

Definition id_of_attribute (a : attribute) : list byte :=
  match a with
  | Attr_string _ s => string_to_list_byte s
  | Attr_ptrofs _ s => string_to_list_byte s end.

Eval compute in reptype t_attr_list.

Definition domain_of_attribute (a : attribute) : val :=
  match a with
    | Attr_ptrofs _ _ => Vint (Int.repr 0)
    | Attr_string _ _ => Vint (Int.repr 1) end.

Definition attribute_string_rep sh (a : attribute) (p : val) : mpred :=
  match a with
  | Attr_ptrofs _ _ => emp
  | Attr_string _ s => cstring sh (string_to_list_byte s) p end.

Fixpoint attribute_lseg_rep (sh : share) (l : list attribute) (p q : val) : mpred :=
  match l with
  | [] => !! (p = q /\ is_pointer_or_null p) && emp
  | a :: tl =>
    EX r : val, EX sptr : val, data_at sh t_attr_list (sptr, (domain_of_attribute a, r)) p * attribute_lseg_rep sh tl r q * attribute_string_rep sh a sptr end.

Lemma attribute_lseg_nil sh l : attribute_lseg_rep sh l nullval nullval |-- !! (l = []).
Proof.
  destruct l; simpl; entailer.
  saturate_local. apply field_compatible_nullval with (P := False) in H.
  contradiction.
Qed.
 
Definition attribute_list_rep sh l p := attribute_lseg_rep sh l p nullval.

Lemma attribute_lseg_rep_local_facts:
  forall sh l p q,
   attribute_lseg_rep sh l p q |--
                      !! (is_pointer_or_null p /\ is_pointer_or_null q).
Proof.
  induction l; intros; unfold attribute_lseg_rep; entailer.
  fold attribute_lseg_rep. sep_apply IHl. entailer.
Qed.

Hint Resolve attribute_lseg_rep_local_facts : saturate_local.

Lemma attribute_list_rep_local_facts:
  forall sh l p,
   attribute_list_rep sh l p |--
                      !! (is_pointer_or_null p /\ (p=nullval <-> l=nil)).
Proof.
  unfold attribute_list_rep, attribute_lseg_rep. induction l; intros; entailer.
  entailer!. easy.
  fold attribute_lseg_rep in IHl |- *.
  sep_apply (IHl r). entailer!. apply field_compatible_nullval1 in H0.
  easy.
Qed.

Hint Resolve attribute_list_rep_local_facts : saturate_local.

Lemma attribute_list_rep_valid_pointer:
  forall sh l p, readable_share sh ->
   attribute_list_rep sh l p |-- valid_pointer p.
Proof.
  intros.
  unfold attribute_list_rep; destruct l; simpl. entailer.
  Intros r sptr. sep_apply data_at_valid_ptr.
  apply readable_nonidentity. assumption.
  entailer.
Qed.

Hint Resolve attribute_list_rep_valid_pointer : valid_pointer.

Definition attributes_rep (sh : share) (attrs : Fset.set FAN) (p : val) : mpred :=
  attribute_list_rep sh ({{{attrs}}}) p.

Definition attribute_list_length_spec :=
 DECLARE _attribute_list_length
  WITH sh : share, l: val, attrs: Fset.set FAN
  PRE [ _l OF (tptr t_attr_list)]
     PROP(readable_share sh)
     LOCAL (temp _l l; temp _x l)
     SEP (attributes_rep sh attrs l)
  POST [ size_t ]
     PROP()
     LOCAL(temp ret_temp (Vint (Int.repr (Z.of_nat (Fset.cardinal _ attrs)))))
     SEP (attributes_rep sh attrs l).

Definition Gprog := ltac:(with_library prog [attribute_list_length_spec]).
Print Fset.
Lemma body_attribute_list_length: semax_body Vprog Gprog f_attribute_list_length attribute_list_length_spec.
Proof.
  start_function.
  forward. forward.
  forward_loop
    (EX x : val, EX l1 : list attribute, EX l2 : list attribute,
            PROP ( {{{attrs}}} = l1 ++ l2 )
                 LOCAL (temp _n (Vint (Int.repr (Zlength l1))); temp _x x; temp _l l)
                 SEP (attribute_lseg_rep sh l1 l x; attribute_list_rep sh l2 x ))%assert.
  - Exists l (@nil attribute) (Fset.elements _ attrs).
  entailer!. unfold attributes_rep, attribute_list_rep. simpl. entailer!.
  - entailer!.
  - destruct l2. unfold attribute_list_rep, attribute_lseg_rep at 2. 
    Intros. rewrite H0 in HRE. contradiction.
    unfold attribute_list_rep; unfold attribute_lseg_rep at 2; fold attribute_lseg_rep. 
    Intros r sptr.
    forward.
    forward.
    Exists (r, l1 ++ [a], l2). entailer!.
    split. rewrite semax_lemmas.cons_app in H. rewrite app_assoc in H. auto.
    rewrite Zlength_app. rewrite Zlength_cons. simpl. reflexivity.
    unfold attribute_list_rep. cancel.
    revert l PNl. generalize l1. induction l0; intros. rewrite app_nil_l. simpl. Exists r sptr. entailer.
    simpl. Intros r0 sptr0. Exists r0 sptr0. entailer!.
    apply IHl0. auto.
  - forward. assert (h : l2 = []). rewrite <- H0; easy.
    subst l2. rewrite app_nil_r in H. subst l1.
    rewrite Fset.cardinal_spec. rewrite Zlength_correct; auto. entailer!.
    unfold attributes_rep, attribute_list_rep. sep_apply attribute_lseg_nil. simpl. entailer!.
Qed.
