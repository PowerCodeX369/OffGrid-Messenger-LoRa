import com.offgrid.messenger.ReceiptStates
import com.offgrid.messenger.ReceiptVisual
fun main(){
    check(ReceiptStates.visual("waiting",1)==ReceiptVisual.WAITING)
    check(ReceiptStates.advance("sent","waiting")=="waiting")
    check(ReceiptStates.advance("delivered","waiting")=="delivered")
    check(ReceiptStates.advance("read","waiting")=="read")
    check(ReceiptStates.advance("waiting","sent")=="sent")
    check(ReceiptStates.advance("failed","sending")=="sending")
    println("PASS: durable text waiting state cannot demote authenticated delivered or read")
}
