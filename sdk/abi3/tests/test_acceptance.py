import sys,unittest
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'tools'))
from record_acceptance import parse
TEXT=(R/'tests/expected-report.txt').read_text()
class Acceptance(unittest.TestCase):
 def test_valid(self):self.assertEqual(parse(TEXT)['Live sessions/bytes'],[0,0])
 def test_refuse_missing_or_contradictory(self):
  for old,new in [('20/20','20/19'),('0/0','1/1432'),('4/4/10','4/3/10'),('48/0','49/0'),('32/32','32/31'),('STOPPED','LIVE'),('HELD','FREED'),('G4R2 SDK PASS','G3 SDK PASS'),('Quiet ticks: 20','Quiet ticks: 0')]:
   with self.subTest(old=old),self.assertRaises(ValueError):parse(TEXT.replace(old,new))
  with self.assertRaises(ValueError):parse(TEXT+'Error: 0\n')
if __name__=='__main__':unittest.main()
