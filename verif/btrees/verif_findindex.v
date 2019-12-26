(** * verif_findindex.v : Correctness proof of findChildIndex and findRecordIndex *)

Require Import VST.floyd.proofauto.
Require Import VST.floyd.library.
Require Import relation_mem.

Require Import VST.msl.wand_frame.
Require Import VST.msl.iter_sepcon.
Require Import VST.floyd.reassoc_seq.
Require Import VST.floyd.field_at_wand.
Require Import FunInd.
Require Import btrees.
Require Import btrees_sep.
Require Import btrees_spec.
Require Import index.

(* Move this to floyd/forward.v? *)
Lemma ltu_false_inv64:
 forall x y, Int64.ltu x y = false -> Int64.unsigned x >= Int64.unsigned y.
Proof.
intros.
unfold Int64.ltu in H. if_tac in H; inv H; auto.
Qed.


Lemma FCI_increase: forall X (le:listentry X) key i,
    idx_to_Z i <= idx_to_Z (findChildIndex' le key i).
Proof.
  intros. generalize dependent i.
  induction le; intros.
  - simpl. omega.
  - destruct e; simpl.
    * destruct (k_ key <? k_ k). omega.
      eapply Z.le_trans with (m:=idx_to_Z (next_index i)). rewrite next_idx_to_Z. omega.
      apply IHle.
    * destruct (k_ key <? k_ k). omega.
      eapply Z.le_trans with (m:=idx_to_Z (next_index i)). rewrite next_idx_to_Z. omega.
      apply IHle.
Qed.

Lemma FCI'_next_index {X: Type} (le: listentry X) key i:
  findChildIndex' le key (next_index i) = next_index (findChildIndex' le key i).
Proof.
  revert i.
  induction le as [|[k v x|k n] le]; simpl; try easy;
    destruct (k_ key <? k_ k); easy.
Qed.  

Lemma FRI'_next_index {X: Type} (le: listentry X) key i:
  findRecordIndex' le key (next_index i) = next_index (findRecordIndex' le key i).
Proof.
  revert i.
  induction le as [|[k v x|k n] le]; simpl; try easy;
    destruct (k_ key <=? k_ k); easy.
Qed.

Lemma FCI_inrange: forall X (n:node X) key,
    -1 <= idx_to_Z(findChildIndex n key) < numKeys n.
Proof.
  intros X n key.
  destruct n as [ptr0 le isLeaf F L x]; simpl.
  induction le. easy. simpl.
  destruct e as [k v x'|k n]; destruct (k_ key <? k_ k);
  replace (findChildIndex' le key 0) with (next_index (findChildIndex' le key im)) by now rewrite <- FCI'_next_index.
  all: destruct (findChildIndex' le key im); unfold  im, findChildIndex', next_index, idx_to_Z in IHle |- *; omega.
Qed.

Lemma FCI_inrange'': forall X (le:listentry X) key j i,
    findChildIndex' le key (ip j) = ip i ->
    j <= i.
Proof.
  intros.
  revert j H; induction le; simpl; intros. unfold ip in H; omega.
  destruct e as [k v x'|k n]; destruct (k_ key <? k_ k); simpl in *; unfold ip in *; try omega.
  1,2: apply IHle in H; unfold next_index in *; omega.
Qed.

(*
Lemma FCI_inrange': 
    forall X (n:node X) key i,
      findChildIndex n key = ip i ->
    0 <= i < numKeys n.
Proof.
intros.
 pose proof (FCI_inrange X n key). rewrite H in H0. simpl in H0.
 unfold idx_to_Z, ip in *.
 destruct (zlt i 0); try omega.
 elimtype False. clear H0.
 destruct n; simpl in *.
 destruct l0; simpl in *. unfold im in *; subst i. inv H.
  destruct e as [k v x'|k n]; destruct (k_ key <? k_ k); simpl in *; try discriminate.
  all: apply FCI_inrange'' in H; omega.
Qed.
*)

Lemma FRI_increase: forall X (le:listentry X) key i,
    idx_to_Z i <= idx_to_Z (findRecordIndex' le key i).
Proof.
  intros. generalize dependent i.
  induction le; intros.
  - simpl. omega.
  - destruct e; simpl.
    * destruct (k_ key <=? k_ k). omega.
      eapply Z.le_trans with (m:=idx_to_Z (next_index i)). rewrite next_idx_to_Z. omega.
      apply IHle.
    * destruct (k_ key <=? k_ k). omega.
      eapply Z.le_trans with (m:=idx_to_Z (next_index i)). rewrite next_idx_to_Z. omega.
      apply IHle.
Qed.

Lemma FRI_inrange: forall X (n:node X) key,
    0 <= idx_to_Z (findRecordIndex n key) <= numKeys n.
Proof.
  intros X n key.
   destruct n as [ptr0 le isLeaf F L x]; simpl.
  unfold idx_to_Z, ip.
  rewrite <- (Z.add_0_r (numKeys_le le)).
  forget 0 as i.
  revert i; induction le; intros. easy.
  simpl.
  unfold next_index.
  pose proof (numKeys_le_nonneg le).
  destruct e as [k v x'|k n]; destruct (k_ key <=? k_ k); try easy; try omega;
  specialize (IHle (Z.succ i));   omega.
Qed.

Lemma body_findChildIndex: semax_body Vprog Gprog f_findChildIndex findChildIndex_spec.
Proof.
  start_function.
  forward.                      (* i=0 *)
  destruct n as [ptr0 le isLeaf First Last pn].
  pose (n:= btnode val ptr0 le isLeaf First Last pn). fold n.
  rewrite unfold_btnode_rep. unfold n. Intros ent_end.
  forward.                      (* t'4=node->numKeys *)
  simpl in H. destruct isLeaf; try inv H.
  sep_apply (fold_btnode_rep ptr0).  fold n.

  forward_if (PROP ( )
     LOCAL (temp _t'4 (Vint (Int.repr (numKeys (btnode val ptr0 le false First Last pn))));
     temp _i (Vint (Int.repr 0)); temp _node (getval (btnode val ptr0 le false First Last pn));
     temp _key (key_repr key))  SEP (btnode_rep n)).
  - forward.                    (* skip *)
    entailer!.
  - apply intern_le_cons in H0. destruct H0. destruct H0. rewrite H0 in H. simpl in H.
     red in H1. simpl in H1. subst le. simpl in H1.
     pose proof (numKeys_le_nonneg x0).
     rewrite Int.signed_repr in H by rep_omega. omega.
     simpl. auto.
  - destruct le as [|e le'] eqn:HLE.
    { apply intern_le_cons in H0. destruct H0. destruct H. inv H. simpl. auto. }
    destruct e eqn:HE.
    simpl in H0.
    destruct ptr0; try inv H0.  (* keyval isn't possible in an intern node *)
    rewrite unfold_btnode_rep. unfold n. simpl. Intros ent_end0.
    forward.                    (* t'6=node->entries[0]->key *)
    change (?A :: ?B ++ ?C) with ((A::B)++C).
    change ((key_repr k, inl (getval n0)) :: le_to_list le') with
        (le_to_list (cons val (keychild val k n0) le')).
   change (btnode_rep n0) with (entry_rep (keychild val k n0)).
    sep_apply cons_le_iter_sepcon.
    change Vfalse with (Val.of_bool false).
    sep_apply (fold_btnode_rep ptr0). fold n.
    deadvars!.      
(*    apply node_wf_numKeys in H1. simpl in H1.*)
{  forward_loop (EX i:Z, PROP(0 <= i <= numKeys n; findChildIndex' le key im = findChildIndex' (skipn_le le i) key (prev_index (ip i))) 
                                     LOCAL(temp _i (Vint(Int.repr i)); temp _node pn; temp _key (key_repr key))
                                     SEP(btnode_rep n))
                   break:(EX i:Z, PROP(i=numKeys n; findChildIndex' le key im = prev_index (ip i))
                                        LOCAL(temp _i (Vint(Int.repr i)); temp _node pn; temp _key (key_repr key))
                                        SEP(btnode_rep n)).

  - Exists 0.
    entailer!. split. omega. apply numKeys_le_nonneg.
  - Intros i. clear ent_end ent_end0.
    rewrite unfold_btnode_rep. unfold n. Intros ent_end.
    forward.                    (* t'5=node->numKeys *)
    sep_apply (fold_btnode_rep ptr0 (cons val (keychild val k n0) le')  false). fold n.
    forward_if.
    + clear ent_end. rewrite unfold_btnode_rep. unfold n. Intros ent_end.
      assert(HRANGE: 0 <= i < numKeys_le le).
      { apply node_wf_numKeys in H1. simpl in H1.
        unfold n in H; simpl in H. rewrite HLE. simpl.
        rewrite !Int.signed_repr in H3 by rep_omega. omega. }
      assert(NTHENTRY: exists ei, nth_entry_le i le = Some ei).
      { apply nth_entry_le_in_range. auto. }
      destruct NTHENTRY as [ei NTHENTRY].
      assert(ZNTH: nth_entry_le i le = Some ei) by auto.
      eapply Znth_to_list with (endle:=ent_end) in ZNTH. 
      assert (H99: 0 <= numKeys_le le <= Fanout). {
         clear - HLE H1. subst le. apply (node_wf_numKeys _ H1).
     }
      forward.                  (* t'2=node->entries+i->key *)
      { apply prop_right. rep_omega. }
      { entailer!. simpl in ZNTH. rewrite ZNTH. destruct ei; simpl; auto. }
      rewrite HLE in ZNTH. rewrite ZNTH.
      forward_if.
      * forward.                (* return i-1 *)
        entailer!.
        { simpl cast_int_int.  normalize. f_equal. f_equal.
          simpl.
          replace (if k_ key <? k_ k then im else findChildIndex' le' key 0) with
              (findChildIndex' (cons val (keychild val k n0) le') key im) by (simpl; auto).
          rewrite H2.
          pose (le:=cons val (keychild val k n0) le').
          fold le. fold le in NTHENTRY.
          clear -NTHENTRY H4 HRANGE.
          assert(k_ key <? k_ (entry_key ei) = true).
          { assert(-1 < k_ key < Ptrofs.modulus) by (unfold k_; rep_omega).
            destruct ei; simpl in H4; simpl;
            apply typed_true_of_bool in H4;
            apply Int64.ltu_inv in H4; apply Zaux.Zlt_bool_true;
            rewrite ?int_unsigned_ptrofs_toint in H4 by reflexivity;
            rewrite ?int64_unsigned_ptrofs_toint in H4 by reflexivity;
            apply H4. }
          apply nth_entry_skipn in NTHENTRY.
          destruct (skipn_le le i); simpl in NTHENTRY; inv NTHENTRY.
          destruct ei; simpl in H; simpl; rewrite H; normalize; f_equal.
          all: unfold rep_index; if_tac; simpl; omega. }
          rewrite unfold_btnode_rep with (n:= btnode val ptr0 (cons val (keychild val k n0) le') false First Last pn).
        Exists ent_end. cancel.
      * forward.                (* i++ *)
        Exists (Z.succ i). entailer!. split.
        { clear - HRANGE H1. subst n. simpl in *. omega. }
        { rewrite H2.
          pose (le:=cons val (keychild val k n0) le').
          fold le. fold le in NTHENTRY. clear -NTHENTRY H4 HRANGE.
          assert(k_ key <? k_ (entry_key ei) = false).
          { assert(-1 < k_ key < Int64.modulus) by (unfold k_; rep_omega).
            apply Zaux.Zlt_bool_false; unfold k_.
            destruct ei; simpl in H4; simpl;
              apply typed_false_of_bool in H4;  apply ltu_false_inv64 in H4;
              rewrite ?int_unsigned_ptrofs_toint in H4 by reflexivity;
              rewrite ?int64_unsigned_ptrofs_toint in H4 by reflexivity;
              omega. }
          apply nth_entry_skipn in NTHENTRY.          
          rewrite skip_S.
          destruct (skipn_le le i); simpl in NTHENTRY; inv NTHENTRY.
          assert(findChildIndex' (cons val ei l) key (prev_index (ip i)) = findChildIndex' l key (next_index (prev_index (ip i)))).
          { simpl; destruct ei; simpl in H; rewrite H; simpl; auto. } rewrite H0.
          simpl. f_equal. unfold next_index, prev_index, ip. omega. omega. }
        do 2 f_equal. replace 1 with (Z.of_nat 1) by reflexivity.
        rewrite unfold_btnode_rep with (n:=n). unfold n. Exists ent_end.
        cancel.
    + forward.                  (* break *)
      unfold n in H. unfold node_wf in H1. simpl in H, H1.
      rewrite Int.signed_repr in H3 by rep_omega.
      rewrite Int.signed_repr in H3 by rep_omega.
      assert( i = Z.succ (numKeys_le le')) by omega.
      Exists i. entailer!.
      rewrite H2. simpl.
      rewrite zle_false by (pose proof (numKeys_le_nonneg le'); omega).
      rewrite Z.pred_succ.
      rewrite skipn_full. simpl. auto. 
  - Intros i. clear ent_end ent_end0.
    rewrite unfold_btnode_rep. unfold n. Intros ent_end.
    forward.                     (* t'1=node->numKeys *)
    forward.                     (* return t'1-1 *)
    + entailer!. unfold node_wf in H1. simpl in H1.
      pose proof (numKeys_le_nonneg le').
      rewrite Int.signed_repr by rep_omega.
      rewrite Int.signed_repr by rep_omega.
      rep_omega.
    + entailer!.
      * simpl cast_int_int; normalize.
        do 2 f_equal.
        unfold findChildIndex. rewrite H2. simpl rep_index. simpl numKeys.
        unfold rep_index; simpl.
        pose proof (numKeys_le_nonneg le').
        unfold prev_index, ip; omega.
      * rewrite unfold_btnode_rep with (n:=btnode val ptr0 (cons val (keychild val k n0) le') false First Last pn).
        Exists ent_end. cancel.  }
Qed.

Lemma body_findRecordIndex: semax_body Vprog Gprog f_findRecordIndex findRecordIndex_spec.
Proof.
  start_function.
  forward.                      (* i=0 *)
  destruct n as [ptr0 le isLeaf First Last pn].
  pose (n:= btnode val ptr0 le isLeaf First Last pn). fold n.
  rewrite unfold_btnode_rep. unfold n. Intros ent_end.
  forward.                      (* t'5=node->numKeys *)
  simpl.
  sep_apply (fold_btnode_rep ptr0). fold n.
  clear ent_end.
  forward_if(PROP ( )
     LOCAL (temp _t'5 (Vint (Int.repr (numKeys_le le)));
     temp _i (Vint (Int.repr 0)); temp _node pn;
     temp _key (key_repr key))  SEP (btnode_rep n)).
  { forward. entailer!. }
  { exfalso. apply node_wf_numKeys in H0. simpl in H0.
    rewrite Int.signed_repr in H1; rep_omega. }
  rewrite unfold_btnode_rep. unfold n. Intros ent_end.
  forward.                      (* t'4=node->numKeys *)
  forward_if.
  { forward.                    (* return 0 *)
    entailer!.
    apply (f_equal Int.unsigned) in H1. rewrite Int.unsigned_repr in H1.
    rewrite Int.unsigned_repr in H1 by rep_omega.
    destruct le.
    simpl. auto. 
    simpl in H1. pose proof (numKeys_le_nonneg le); omega.
    apply node_wf_numKeys in H0. simpl in H0. rep_omega.
    rewrite unfold_btnode_rep with (n:=btnode val ptr0 le isLeaf First Last pn).
    Exists ent_end. entailer!. }
  forward.                    (* i=0 *)
  simpl.
  sep_apply (fold_btnode_rep ptr0). fold n.
  clear ent_end. deadvars!.
{ forward_loop (EX i:Z, PROP(0<=i<=numKeys n; findRecordIndex' le key (ip 0) = findRecordIndex' (skipn_le le i) key (ip i))
                                    LOCAL (temp _i (Vint (Int.repr i)); temp _node pn; temp _key (key_repr key))
                                    SEP (btnode_rep n))
               break:(EX i:Z, PROP(i=numKeys n; findRecordIndex' le key (ip 0) = ip i) 
                                    LOCAL (temp _i (Vint (Int.repr i)); temp _node pn; temp _key (key_repr key))
                                    SEP (btnode_rep n)).
  - Exists 0. entailer!.
    split. split. omega. apply numKeys_le_nonneg.
    rewrite skipn_0. auto.
  - Intros i. rewrite unfold_btnode_rep. unfold n. Intros ent_end.
    forward.                    (* t'3=node->numKeys *)
    forward_if.
    + entailer!.
      apply node_wf_numKeys in H0. simpl in H0.
      rewrite Int.signed_repr by rep_omega.
      rewrite Int.signed_repr by rep_omega.
      rep_omega.
    + apply node_wf_numKeys in H0; simpl in H0. unfold n in H2; simpl in H2.
        assert(HRANGE: 0 <= i < numKeys_le le).
      { rewrite !Int.signed_repr in H4 by rep_omega. omega. }
      assert(NTHENTRY: exists ei, nth_entry_le i le = Some ei).
      { apply nth_entry_le_in_range. auto. }
      destruct NTHENTRY as [ei NTHENTRY].
      assert(ZNTH: nth_entry_le i le = Some ei) by auto.
      eapply Znth_to_list with (endle:=ent_end) in ZNTH.
      forward.                  (* t'2=node->entries[i]->key *)
      { entailer!. }
      { entailer!. rewrite ZNTH. destruct ei; simpl; auto. }
      rewrite ZNTH.
      forward_if.
      * forward.                (* return i *)
        entailer!. unfold findRecordIndex. rewrite H3.
        f_equal. f_equal.
        destruct (skipn_le le i) eqn:HSKIP.
        { simpl. auto. }
        apply nth_entry_skipn in NTHENTRY.
        simpl in  NTHENTRY. rewrite HSKIP in NTHENTRY. inv NTHENTRY.
        assert(k_ key <=? k_ (entry_key ei) = true).
        { assert(-1 < k_ key < Int64.modulus) by (unfold k_; rep_omega).
          destruct ei; simpl in H5; simpl;
            apply typed_true_of_bool in H5;
            apply binop_lemmas3.negb_true in H5;
            apply ltu_false_inv64 in H5;
              rewrite ?int_unsigned_ptrofs_toint in H5 by reflexivity;
              rewrite ?int64_unsigned_ptrofs_toint in H5 by reflexivity;
            try apply Zaux.Zle_bool_true; unfold k_; omega. }
        simpl. destruct ei; simpl in H10; rewrite H10.
        simpl. auto. simpl. auto.
        rewrite unfold_btnode_rep with (n:=btnode val ptr0 le isLeaf First Last pn).
        Exists ent_end. entailer!.
      * forward.                (* i=i+1 *)
        Exists (Z.succ i). entailer!.
        split.
        { unfold n; simpl; omega. }
        { unfold ip in H3; rewrite H3. clear -NTHENTRY H5 HRANGE.
          assert(k_ key <=? k_ (entry_key ei) = false).
          { assert(-1 < k_ key < Int64.modulus) by (unfold k_; rep_omega).
            destruct ei; simpl in H5; simpl;
              apply typed_false_of_bool in H5;
              apply negb_false_iff in H5;
              apply Int64.ltu_inv in H5;
              rewrite ?int_unsigned_ptrofs_toint in H5 by reflexivity;
              rewrite ?int64_unsigned_ptrofs_toint in H5 by reflexivity;
              try apply Zaux.Zle_bool_false; unfold k_; omega. }
          apply nth_entry_skipn in NTHENTRY.          
          rewrite skip_S.
          destruct (skipn_le le i); simpl in NTHENTRY; inv NTHENTRY.
          simpl. destruct ei; simpl; simpl in H; rewrite H; auto. omega. }
        rewrite unfold_btnode_rep with (n:=n). unfold n.
        Exists ent_end. entailer!.
    + forward.                  (* break *)
      Exists (numKeys_le le).
      entailer!.
      assert(i=numKeys_le le).
      { unfold n in H2. simpl in H2.
      apply node_wf_numKeys in H0. simpl in H0.
        rewrite !Int.signed_repr in H4 by rep_omega.
        rep_omega. }
      subst. split.
      * unfold ip in H3; rewrite H3. rewrite skipn_full.
        simpl. auto.
      * auto.
      * rewrite unfold_btnode_rep with (n:=n).
        unfold n. Exists ent_end. entailer!.
  - Intros i. subst.
    rewrite unfold_btnode_rep. unfold n. Intros ent_end.
    forward.                    (* t'1=node->numkeys *)
    forward.                    (* return t'1 *)
    entailer!.
    + f_equal. f_equal. unfold findRecordIndex. rewrite H3. simpl. auto.
    + rewrite unfold_btnode_rep with (n:=btnode val ptr0 le isLeaf First Last pn).
      Exists ent_end. entailer!. }
Qed.
