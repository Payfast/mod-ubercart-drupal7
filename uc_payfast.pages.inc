<?php

function uc_payfast_complete( $cart_id = 0 )
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

function uc_payfast_itn()
{
    
    require_once('payfast_common.inc');
    // Check what mode the site is in. 
    if (variable_get('uc_payfast_mode', '') == 'sandbox') {
        $pfHost = 'https://sandbox.payfast.co.za';
    }
    else {
        $pfHost = 'https://www.payfast.co.za';
    }
    
    watchdog( 'payfast', 'Checking remote_ip : ' );

    if( !pfValidIP( $_SERVER['REMOTE_ADDR'] ) ) {
        watchdog( 'payfast', 'PayFast REMOTE_IP mismatch: ');
        return false;
    }

    $data= pfGetData();
    
    watchdog( 'payfast', 'PayFast Checking POST data sent from Payfast: '. print_r( $data, true ) );

    if( $data === false )
    {
        watchdog( 'payfast', 'PayFast Post data empty: '. print_r( $data, true ) );
        return false;
    }
    
    if( !pfValidSignature( $data) )
    {
        watchdog( 'payfast', 'PayFast signature mismatch: ');
        return false;
    }
    
    $request='';
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
        pflog( "ITN POST data integrity mismatch for $request\n");
        return false;
    } 
        
    $order = uc_order_load( $data['m_payment_id'] );
    
	if ($order == FALSE) {
		watchdog('uc_payfast', 'IPN attempted for non-existent order.', array(), WATCHDOG_ERROR);
		return false;
	}
    
	if (variable_get('uc_payfast_merchant_id', '') != $data['merchant_id']) {
        watchdog( 'payfast', 'ITN received for a different merchant_id 
         | Order Number: ' . $data['m_payment_id'] . ' | Payfast Transaction ID: ' . $data['pf_payment_id'] );
        return false;
	}
	
	//check that the amount paid is the same as the order total
    if ( ( int )$data[amount_gross] != ( int )$order->order_total )
    {
        watchdog( 'payfast', 'The amount paid <> the order amount. 
         | Order Number: ' . $data['m_payment_id'] . ' | Payfast Transaction ID: ' . $data['pf_payment_id'] );
        return false;
    }
    
    //check that the order status is: "in_checkout"
    if ( $order->order_status != 'in_checkout' )
    {
        watchdog( 'payfast', 'The order status was not in_checkout when receiving this ITN. | Order Number: ' . 
		$data['m_payment_id'] .' | Payfast Transaction ID: ' . $data['pf_payment_id'] );
        return false;
    }

    //Update order status from 'pending' to 'Payment received'    
    uc_payment_enter($order->order_id, 'payfast', $order->order_total, $order->uid, null, $comment );
	
    //Process payment = insert statement in invoice table
	uc_cart_complete_sale($order);

    pflog( "ITN sucessfully verified for Payfast Transaction ID: {$data['pf_payment_id']}");
    return true;
}

function uc_payfast_cancelled()
{
    return '<p>You cancelled the payment. You may <a href="/cart">try again</a>.</p>';
}