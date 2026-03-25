my @movesets = qw(R,U,F,L,BR,BL,FR,FL,DR,DL,B,D R,U,F,L,BR,BL,FR,FL,DR
   R,U,F,L,BR,BL,FR,FL R,U,F,L,BR,BL,FR R,U,F,L,BR,BL R,U,F,L,BR R,U,F,L
   R,U,F R,U) ;
my $scramble = "L U BL DL FL DL D FR BL' D B DR B BL U BR L F U F R F2 U' L2' U2 L U2 F L F2' L' F2' B2' DL' B2 DL' D' DL2 D' DL2' B' DL2 B' DL' B2 DL' D2' B DL2 B D2' B2' DL' D2' B'" ;
for ($step=0; $step<@movesets; $step++) {
   print "Step $step\n" ;
   $moves = $movesets[$step] ;
   if ($step+1 < @movesets) {
      $subgroup = $movesets[$step+1] ;
      $cmd = "build/bin/twsearch --scramblealg \"$scramble\" -a 0 -M 1000 --nowrite --moves $moves --subgroupmoves $subgroup megaminx.tws" ;
   } else {
      $cmd = "build/bin/twsearch --scramblealg \"$scramble\" -a 0 -M 1000 --nowrite --moves $moves megaminx.tws" ;
   }
   print("$cmd\n") ;
   open F, "$cmd |" or die "Can't spawn" ;
   $sol = undef ;
   while (<F>) {
      print ;
      chomp ;
      if (!defined($sol) && /^ [A-Z]/) {
         $sol = $_ ;
      }
   }
   close F ;
   die "Couldn't solve?" if !defined($sol) ;
   $scramble .= $sol ;
}
print "Final solution is $scramble\n" ;
