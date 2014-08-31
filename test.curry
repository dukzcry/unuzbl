import CLPFD
import qualified CLPR
import AllSolutions
import GraphInductive
import Unsafe
import Random
import GUI
import Float
import Maybe
import Function

import List

gen_vars n = if n==0 then [] else var : gen_vars (n-1)  where var free
undir = gmap (\(p,v,l,s)->let ps = nub (p++s) in (ps,v,l,ps))

intersectBy _ []     _     = []
intersectBy eq (x:xs) ys    = if (eq x) `any` ys then x : intersectBy eq xs ys
                                        else intersectBy eq xs ys

αβ = [2,5]

data edge = a | b | c | d

junc 1 e = case e of
  a -> a
  b -> b
  c -> c
  d -> d
junc 2 e = case e of
  a -> b
  b -> a
  c -> d
  d -> c
junc 3 e = case e of
  a -> b
  b -> d
  c -> a
  d -> c
junc 4 e = case e of
  a -> c
  b -> a
  c -> d
  d -> b
junc 5 e = case e of
  a -> c
  b -> d
  c -> a
  d -> b
junc 6 e = case e of
  a -> a
  b -> c
  c -> b
  d -> d

pat [a,b,c,d] = success
pat [d,c,b,a] = success
pat [b,d,a,c] = success
pat [c,a,d,b] = success
pat [c,d,a,b] = success

js n = let
       l = unsafePerformIO (getAllSolutions js_)
       js_ o =
   	   t =:= gen_vars n & domain t 1 6 & labeling [] t &
 	   o =:= map (\ t -> (\ e -> junc t e)) t & jc =:= foldl1 (flip (.)) o & pat (map jc [a,b,c,d])
           where t,jc free
   in l !! head (take 1 (nextIntRange (unsafePerformIO getRandomSeed) (length l)))

edges [[x1,y1],[x2,y2]] =
   let A = y1 -. y2
       B = x2 -. x1
       c1 = x1*.y2 -. x2*.y1
       y C = (\ x -> (0.0 CLPR.-.A CLPR.*.x CLPR.-. C) CLPR./. B)
       eq C = (\ y x -> A CLPR.*.x CLPR.+. B CLPR.*.(y x) CLPR.+. C =:= 0)
       z1 = sqrt (A^.2 +. B^.2)
       Cs d_ =
   	   let z = d_ *. z1
   	   in map (\ C -> (y C,eq C)) [c1 +. z, c1 -. z]
   in concatMap Cs αβ -- [b,c,a,d]

corner j [(_,_,es1)] [(_,_,es2)] = let
       	 cross (y,_) (_,eq) = let
	 	       x = fromJust (unsafePerformIO (getOneSolution (eq y)))
       	 	      in both round (x,y x) -- direct calc of y produces suspended constraint as side effect
	 corner_ e [b_,c_,a_,d_] = case e of
		a -> a_
		b -> b_
		c -> c_
		d -> d_
       in zipWith (\ e v -> cross v (corner_ (j e) es2)) [b,c,a,d] es1

main = runGUI "" widget

widget = let
	l = [[[-100,-90],[-70,110]],[[-70,110],[110,-100]],[[110,-100],[-100,-90]]]

     	n = length l
	is = [1..n]
     	bg = mkGraph (zip is (js n)) [if i == n then (n,1,j) else (i,i+1,j) | (i,j) <- zip is (map edges l)]
	gu = undir bg
	fg (p,v,jf,s) = (p,v,(jf,(out gu v)),s) -- (p,v,j(p,s),s)
	g1 = gmap (\ ctx -> fg ctx) bg
	fixup (jf,l1) (jf1,l2) =
	      let common = (intersectBy (\ (i,k,v) e2 -> (k,i,v) == e2) l1 l2)
	      	  fil = (\ (i,k,v) -> [(i,k,v)] /= common && [(k,i,v)] /= common)
	      in zip (corner jf common (filter fil l1)) (corner jf1 common (filter fil l2))
	ln nod j1 = case nod of
      	      [(_,n)] -> let j = fromJust (lab g1 n) in [(fixup j j1,n)]
      	      _ -> []
	fg1 (p,v,j,s) = ((ln p j),v,j,(ln s j)) -- (pnts(p),v,l,pnts(s))
	g2 = gmap (\ ctx -> fg1 ctx) g1
	draw ((x1,y1),(x2,y2)) gport = do
	     addCanvas cref [CLine [(x1+200,y1+200),(x2+200,y2+200)] ""] gport
	forlabs (_,_,l) gport = do
		 mapIO_ (\ ln -> (draw ln gport)) l
	foredges gport = do
	     mapIO_ (\ e -> (forlabs e gport)) (labEdges g2)
     in Col [] [Canvas [WRef cref, Height 400, Width 400],
       	      Button foredges [Text "Draw"]]
	      where cref free
