<?php
/**
 * @param int $cart_id
 * @return string
 */

function uc_pfpayfast_complete( $cart_id = 0 )
{
	global $user;

	$cart_id = uc_cart_get_id();
	$order_id = $_SESSION['cart_order'];
	
	//empty the shopping cart
	uc_cart_empty($cart_id);

	$output = "The Order Number for this order is $order_id.<br>".
	"View <a href='/user/".$user->uid."/orders/$order_id'>this order</a><br>".
	"View <a href='/user/".$user->uid."/orders'>all my orders</a>";
	
	return $output;
}

function uc_pfpayfast_itn()
{
    
    require_once('payfast_common.inc');
    // Check what mode the site is in. 
    if (variable_get('uc_pfpayfast_mode', '') == 'sandbox') {
        $pfHost = 'https://sandbox.payfast.co.za';
    }
    else {
        $pfHost = 'https://www.payfast.co.za';
    }
    
    watchdog( 'payfast', 'ITN received from payfast.co.za ' );

    if( !pfValidIP( $_SERVER['REMOTE_ADDR'] ) ) {
        watchdog( 'payfast', 'REMOTE_IP mismatch: ');
        return false;
    }

    $data= pfGetData();
    
    watchdog( 'payfast', 'POST received from payfast.co.za: '. print_r( $data, true ) );

    if( $data === false )
    {
        watchdog( 'payfast', 'POST is empty: '. print_r( $data, true ) );
        return false;
    }
    
    if( !pfValidSignature( $data) )
    {
        watchdog( 'payfast', 'Signature mismatch on POST');
        return false;
    }
    
    watchdog( 'payfast', 'Signature OK');

    $itnPostData = array();
    $itnPostDataValuePairs = array();
    
    foreach ($_POST as $key => $value) 
    {
        if($key == 'signature')
            continue;
            
	    $value = urlencode(stripslashes($value));
        $value = preg_replace('/(.*[^%^0^D])(%0A)(.*)/i', '${1}%0D%0A${3}', $value);
        
        $itnPostDataValuePairs[] = "$key=$value";
	}
    
    $itnVerifyRequest = implode ('&', $itnPostDataValuePairs);
    
    if (! pfValidData($pfHost, $itnVerifyRequest, "$pfHost/eng/query/validate")) {            
        pflog( "ITN mismatch for $itnVerifyRequest\n");
    	watchdog( 'payfast', 'ITN not OK');
        return false;
    } 
        
    watchdog( 'payfast', 'ITN OK');

    pflog( "ITN verified for $itnVerifyRequest\n");

if (false)
    db_insert('uc_pfpayfast_itn')
      ->fields(array(
        'order_id' => $data['m_payment_id'],
        'txn_id' => $data['pf_payment_id'],
        'txn_type' => $txn_type,
        'mc_gross' => $payment_amount,
        'status' => $data['payment_status'],
        'receiver_email' => $receiver_email,
        'payer_email' => $payer_email,
        'received' => REQUEST_TIME,
      ))
      ->execute();

    $order = uc_order_load( $data['m_payment_id'] );
    
    if ($order == FALSE) {
	watchdog('payfast', "No order found for order_id {$data['m_payment_id']}");
	return false;
    }
    
    watchdog('payfast', "Order found order_id ({$data['m_payment_id']})");

    if (variable_get('uc_pfpayfast_merchant_id', '') != $data['merchant_id']) {
	watchdog( 'payfast', 'ITN received for a different merchant_id | Order Number: ' . $data['m_payment_id'] . ' | Payfast Transaction ID: ' . 
	$data['pf_payment_id'] );
        return false;
	}
	
	//check that the amount paid is the same as the order total
    if ( round( $data['amount_gross'], 2 ) != round( $order->order_total, 2 ) )
    {
        watchdog( 'payfast', 'The amount paid <> the order amount. | Order Number: ' . $data['m_payment_id'] . ' | Payfast Transaction ID: ' . 
	$data['pf_payment_id'] );
        return false;
    }
    
    //check that the order status is: "in_checkout"
    if ( $order->order_status != 'in_checkout' )
    {
        watchdog( 'payfast', 'The order status was not in_checkout when receiving this ITN. | Order Number: ' . $data['m_payment_id'] .
	' | Payfast Transaction ID: ' . $data['pf_payment_id'] );
        return false;
    }

    //Update order status from 'pending' to 'Payment received'    
    uc_payment_enter($order->order_id, 'payfast', $order->order_total, $order->uid, null, $comment );
	
    //Process payment = insert statement into invoice table
    uc_cart_complete_sale($order);

    uc_order_comment_save($order->order_id, 0, t('Payment of @pfcurrency @pfamount received', array('@pfamount' => uc_currency_format($order->order_total, FALSE), '@pfcurrency' => $order->payment_currency)));
    watchdog( 'payfast', "ITN and order successfully processed for order_id {$order->order_id} with order total {$order->order_total}");
    pflog( "ITN sucessfully verified for Payfast Transaction ID: {$data['pf_payment_id']}");
    return true;
}

function uc_pfpayfast_cancelled()
{
    return '<p>You cancelled the payment. You may <a href="/cart">try again</a>.</p>';
}
