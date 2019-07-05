Require Import VST.floyd.proofauto.
Require Import inthash.
Require FMapWeakList.
Require Import Structures.OrdersEx.
Instance CompSpecs : compspecs. make_compspecs prog. Defined.
Definition Vprog : varspecs. mk_varspecs prog. Defined.

Set Default Timeout 5. 
Require Import VST.msl.iter_sepcon.
Require Import VST.floyd.library.

Definition V := block.
Definition N: Z := 10.

Module int_table := FMapWeakList.Make Z_as_DT.

Section IntTable.
  Import int_table.
  Definition get_buckets (m: t V): list (list (Z*V)) :=
    List.map (fun n => filter (fun '(k, v) => eq_dec (k mod N) (Z.of_nat n)) (elements m)) (seq 0%nat (Z.to_nat N)).

  Lemma Zlength_get_buckets: forall m, Zlength (get_buckets m) = N.
  Proof.
    intros []. cbn.
    rewrite Zlength_map, Zlength_correct, seq_length. reflexivity.
  Qed.
  
  Lemma get_buckets_spec (m: t V):
    forall k v, MapsTo k v m <-> List.In (k, v) (Znth (k mod N) (get_buckets m)).
  Proof.
    intros.
    unshelve epose proof (Z_mod_lt k N _). unfold N. computable.
    assert (N = Zlength (seq 0%nat (Z.to_nat N))). rewrite Zlength_correct, seq_length. reflexivity.
    assert (mapsto_iff: MapsTo k v m <-> SetoidList.InA (@eq_key_elt _) (k, v) (elements m)).
    split; [apply elements_1 | apply elements_2].
    rewrite mapsto_iff, SetoidList.InA_alt.
    destruct m as [l nodup]. unfold get_buckets.
    rewrite Znth_map, filter_In by omega.
    rewrite <- nth_Znth by omega.
    rewrite seq_nth by now apply Z2Nat.inj_lt.
    cbn. rewrite Z2Nat.id by omega. destruct initial_world.EqDec_Z.
    { split.
      + intros [[k' v'] [[] hin]].
        cbn in *. now subst.
      + intros [hin _]. exists (k, v). easy. }
    contradiction.
  Qed.

  Lemma MapsTo_inj: forall (m: t V) k v1 v2,
      MapsTo k v1 m -> MapsTo k v2 m -> v1 = v2.
  Proof.
    intros * h1 h2.
    apply find_1 in h1. apply find_1 in h2.
    rewrite h1 in h2. now inv h2.
  Qed.
End IntTable.

Definition t_icell := Tstruct _icell noattr.
Definition t_inthash := Tstruct _inthash noattr.

Fixpoint icell_rep (sh: share) (l: list (Z*V)) (p: val): mpred :=
  match l with
  | [] => !!(p = nullval) && emp
  | (k, v) :: tl => EX q: val,
                    !! (0 <= k < Int.max_unsigned) &&
                    data_at sh t_icell (Vint (Int.repr k), (Vptr v Ptrofs.zero, q)) p *
                    icell_rep sh tl q
  end.

Definition icell_rep_local_facts:
  forall sh l p, icell_rep sh l p |-- !! (is_pointer_or_null p /\ (p=nullval <-> l = [])).
Proof.
  intros sh l. induction l as [ | [] tl]; intro p; simpl.
  + entailer!. easy.
  + Intros q. entailer!. now apply field_compatible_nullval1 in H0.
Qed.
Hint Resolve icell_rep_local_facts: saturate_local.

Definition icell_rep_valid_pointer:
  forall sh l p, readable_share sh -> icell_rep sh l p |-- valid_pointer p.
Proof. intros sh [|[]] p h; cbn; entailer. Qed.
Hint Resolve icell_rep_valid_pointer: valid_pointer.

Definition inthash_rep (sh: share) (m: int_table.t V) (p: val): mpred :=
  let buckets := get_buckets m in
  EX buckets_p: list val,
  !! (Zlength buckets_p = N) &&
     malloc_token sh t_inthash p *
  data_at sh t_inthash buckets_p p *
  iter_sepcon (prod_curry (icell_rep sh)) (combine buckets buckets_p).

Definition inthash_new_spec: ident * funspec :=
   DECLARE _inthash_new
 WITH gv: globals
 PRE [ ] 
   PROP()
   LOCAL(gvars gv)
   SEP(mem_mgr gv)
 POST [ tptr t_inthash ] 
   EX p:val,
      PROP() 
      LOCAL(temp ret_temp p) 
      SEP(inthash_rep Ews (int_table.empty V) p; mem_mgr gv).

Definition new_icell_spec: ident * funspec :=
   DECLARE _new_icell
 WITH gv: globals, key: Z, value: V, pnext: val, tl: list (Z*V)
 PRE [ _key OF tuint, _value OF tptr tvoid, _next OF tptr t_icell ] 
   PROP( 0 <= key < Int.max_unsigned )
   LOCAL(gvars gv; temp _key (Vint (Int.repr key)); temp _value (Vptr value Ptrofs.zero); temp _next pnext)
   SEP(icell_rep Ews tl pnext; mem_mgr gv)
 POST [ tptr t_icell ] 
   EX p:val,
      PROP() 
      LOCAL(temp ret_temp p) 
      SEP(icell_rep Ews ((key, value) :: tl) p; malloc_token Ews t_icell p; mem_mgr gv).

Definition inthash_lookup_spec: ident * funspec :=
   DECLARE _inthash_lookup
 WITH gv: globals, m: int_table.t V, key: Z, pm: val
 PRE [ _p OF tptr t_inthash, _key OF tuint ] 
   PROP(0 <= key < Int.max_unsigned)
   LOCAL(gvars gv; temp _p pm; temp _key (Vint (Int.repr key)))
   SEP(mem_mgr gv; inthash_rep Ews m pm)
 POST [ tptr tvoid ] 
   EX p:val,
      PROP(match int_table.find key m with
             | Some res => p = Vptr res Ptrofs.zero
             | None => p = Vnullptr
          end) 
      LOCAL(temp ret_temp p) 
      SEP(mem_mgr gv; inthash_rep Ews m pm).

Definition Gprog : funspecs :=
        ltac:(with_library prog [ inthash_new_spec; new_icell_spec; inthash_lookup_spec ]).

Lemma body_inthash_lookup: semax_body Vprog Gprog f_inthash_lookup inthash_lookup_spec.
Proof.
  start_function.
  unfold inthash_rep.
  Intros buckets_p.
  remember (combine (get_buckets m) buckets_p) as l.
  remember (key mod N) as i.
  remember (iter_sepcon (prod_curry (icell_rep Ews)) l) as buckets_rep eqn:hb.
  pose proof hb as save_buckets_rep.
  replace l with (sublist 0 i l ++ [(Znth i (get_buckets m), Znth i buckets_p)]
                          ++ sublist (i+1) (Zlength l) l) in hb.
  do 2 rewrite iter_sepcon_app in hb.
  simpl iter_sepcon in hb.
  subst buckets_rep.
  Intros.
  forward.
  { pose proof (Z_mod_lt key 10).
    entailer. }
  { 
    change 10 with N.
    rewrite <- Heqi.
    remember (Znth i buckets_p) as q0.
    remember (Znth i (get_buckets m)) as kvs.
    forward_while (
        EX kvs1 kvs2: list (Z * V), EX q: val,
     PROP ( kvs = kvs1 ++ kvs2 /\ Forall (fun '(k, v) => k <> key) kvs1)
     LOCAL (temp _q q; gvars gv; temp _p pm; temp _key (Vint (Int.repr key)))
     SEP (mem_mgr gv; malloc_token Ews t_inthash pm; data_at Ews t_inthash buckets_p pm;
     iter_sepcon (prod_curry (icell_rep Ews)) (sublist 0 i l);
     icell_rep Ews kvs2 q;
     icell_rep Ews kvs2 q -* icell_rep Ews kvs q0;
     emp; iter_sepcon (prod_curry (icell_rep Ews)) (sublist (i + 1) (Zlength l) l)))%assert.
    + Exists (@nil (Z*V)). Exists kvs. Exists q0.
      entailer!. apply wand_refl_cancel_right.
    + entailer!.
    + destruct kvs2. assert_PROP (q = nullval). entailer!. intuition.
      subst q. contradiction.
      destruct p as [k v].
      unfold icell_rep at 2; fold icell_rep.
      Intros q1. forward.
      forward_if.
      - forward. 
        forward.
        Exists (Vptr v Ptrofs.zero).
        replace (int_table.find key m) with (Some v).
        entailer!.
        unfold inthash_rep. Exists buckets_p.
        rewrite <- save_buckets_rep.
        entailer!.
        eapply modus_ponens_wand'. unfold icell_rep at 2; fold icell_rep.
        Exists q1. entailer. 
        symmetry. eapply int_table.find_1. rewrite get_buckets_spec.
        rewrite (proj1 H1). apply in_or_app. right. constructor. reflexivity.
      - forward. Exists (kvs1 ++ [(k,v)], kvs2, q1).
        entailer!.
        destruct H1. split.
        rewrite <- app_assoc. unfold app at 2. assumption.
        rewrite Forall_app. split. assumption. constructor. assumption. constructor.
        cancel. rewrite <- wand_sepcon_adjoint.
        rewrite sepcon_comm, <- sepcon_assoc.
        apply modus_ponens_wand'. unfold icell_rep at 2; fold icell_rep.
        Exists q1. entailer!.
      + forward. Exists Vnullptr.
        replace (int_table.find (elt := V) key m) with (@None block).
        unfold inthash_rep. entailer!. Exists buckets_p.
        rewrite <- save_buckets_rep. entailer!. apply modus_ponens_wand.
        replace kvs2 with (@nil (Z*V)) in H1 by intuition.
        rewrite app_nil_r in H1.
        remember (int_table.find (elt := V) key m) as f. destruct f.
        symmetry in Heqf. apply int_table.find_2 in Heqf.
        rewrite get_buckets_spec in Heqf.
        destruct H1 as [hnth hforall].
        rewrite hnth in Heqf. rewrite Forall_forall in hforall.
        apply hforall in Heqf. contradiction. reflexivity.
  }
  assert (hlenl: Zlength l = N).
  { subst l. rewrite Zlength_correct, combine_length, Min.min_l, <- Zlength_correct, Zlength_get_buckets.
    reflexivity.
     apply Nat2Z.inj_le. rewrite <- Zlength_correct, <- Zlength_correct, H0, Zlength_get_buckets. omega. }
  { symmetry.
  rewrite <- (sublist_same_gen 0 (Zlength l) l) at 1 by omega. rewrite hlenl.
  assert (0 <= i < 10). subst i. apply Z_mod_lt. computable.
  assert (N = 10) by reflexivity. 
  erewrite sublist_split. f_equal.
  erewrite sublist_split. f_equal.
  rewrite sublist_len_1. subst l. rewrite <- nth_Znth, <- nth_Znth, <- nth_Znth, combine_nth.
  reflexivity.
  apply Nat2Z.inj.
  now rewrite <- Zlength_correct, <- Zlength_correct, Zlength_get_buckets.
  rep_omega.
  rewrite Zlength_get_buckets. omega.
  rewrite Zlength_correct, combine_length. split. easy.
  rewrite Min.min_r. rewrite <- Zlength_correct. omega. 
  apply Nat2Z.inj_le. rewrite <- Zlength_correct, <- Zlength_correct, H0, Zlength_get_buckets. omega.
  omega.  omega. omega. omega. omega. }
Qed.

Lemma body_inthash_new: semax_body Vprog Gprog f_inthash_new inthash_new_spec.
Proof.
  start_function.
  forward_call (t_inthash, gv).
  { split3; auto. cbn. computable. }
  Intros vret.
  forward_if
    (PROP ( )
     LOCAL (temp _p vret; gvars gv)
     SEP (mem_mgr gv; malloc_token Ews t_inthash vret * data_at_ Ews t_inthash vret)).
  { destruct eq_dec; entailer. }
  { forward_call tt. entailer. }
  { forward. rewrite if_false by assumption. entailer. }
  { forward_for_simple_bound N
      (EX i: Z,
       PROP ( 0 <= i <= N )
       LOCAL (temp _p vret; gvars gv)
       SEP (mem_mgr gv;
              malloc_token Ews t_inthash vret;
              data_at Ews t_inthash (list_repeat (Z.to_nat i) nullval ++ list_repeat (Z.to_nat (N-i)) Vundef) vret))%assert.
    + unfold N. omega.
    + now compute.
    + entailer!. unfold N. omega.
      rewrite Z2Nat.inj_0. unfold list_repeat at 1.
      rewrite app_nil_l. entailer.
    + forward.
      entailer!.
      autorewrite with sublist.
      rewrite upd_Znth0, semax_lemmas.cons_app.
      rewrite <- list_repeat_app', <- app_assoc.
      autorewrite with sublist.
      change (list_repeat (Z.to_nat 1) nullval) with [Vint (Int.repr 0)].
      replace (N-(i+1)) with (N-i-1) by omega.
      apply derives_refl. omega. omega.
    + change (list_repeat (Z.to_nat (N - N)) Vundef) with (@nil val).
      rewrite app_nil_r.
      forward. Exists vret. unfold inthash_rep, int_table.empty.
      entailer.
      Exists (list_repeat (Z.to_nat N) nullval).
      rewrite iter_sepcon_emp'.
      entailer!.
      { intros [l p] hxl. pose proof hxl as hxr.
        apply in_combine_l in hxl. apply in_combine_r in hxr.
        cbn in hxl.
        apply list_in_map_inv in hxl. apply in_list_repeat in hxr.
        destruct hxl as [? []].
        subst. cbn.
        apply pred_ext; entailer. }}
Qed.

Lemma body_new_icell: semax_body Vprog Gprog f_new_icell new_icell_spec.
Proof.
  start_function.
  forward_call (t_icell, gv).
  { now compute. }
  Intro vret.
  forward_if (PROP ( )
     LOCAL (temp _p vret; gvars gv; temp _key (Vint (Int.repr key)); temp _value (Vptr value Ptrofs.zero);
     temp _next pnext)
     SEP (mem_mgr gv; malloc_token Ews t_icell vret * data_at_ Ews t_icell vret;
     icell_rep Ews tl pnext)).
  + destruct eq_dec; entailer.
  + forward_call tt. contradiction.
  + forward. rewrite if_false by assumption. entailer.
  + Intros. do 4 forward. Exists vret. cbn. entailer.
    Exists pnext. entailer!.
Qed.

    
