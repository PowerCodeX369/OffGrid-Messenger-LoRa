import com.offgrid.messenger.WeakTextRetry

fun main(){
  val q=WeakTextRetry()
  check(!q.due("id",1000L,false))
  check(q.due("id",1000L,true))
  q.attempt("id",1000L)
  check(!q.due("id",15999L,true))
  check(q.due("id",16000L+WeakTextRetry.jitter("id"),true))
  q.attempt("id",16000L)
  check(!q.due("id",45000L,true))
  check(q.due("id",46000L+WeakTextRetry.jitter("id"),true))
  q.holdForReceipt("id",46000L,true)
  check(!q.due("id",285999L,true))
  check(q.due("id",286000L,true))
  q.acknowledged("id")
  check(q.pendingCount()==0)
  check(q.due("id",286000L,true))
  check(WeakTextRetry.delay(30)==300000L)
  check(WeakTextRetry.receiptWait(true)>WeakTextRetry.receiptWait(false))
  println("PASS: Android encrypted-history weak-link retry eligibility, exponential backoff, Ultra receipt wait, confirmed ACK stop")
}
