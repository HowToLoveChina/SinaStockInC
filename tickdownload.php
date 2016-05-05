<?php

$fc = file("stock_basic.txt");

$begin = mktime(0,0,0,1,1,2000);
$end = time()+3600*24;
@mkdir("tick");
$pids = array();

for($t = $end ;$t >= $begin ; $t -= 3600*24){
	if( date("w",$t) > 5 )continue;
	//! 工作日
	$ymd = date("Y-m-d",$t);
	@mkdir("tick/$ymd");
	printf("%s %s\n",date("H:i:s"),$ymd);
	$cnt= 0;
	$all=sizeof($fc);
	$ect= 0;
	foreach($fc as $stock){
		$cnt++;
		printf("%d/%d/%d\r",$cnt,$ect,$all);
		$stock = trim($stock);
		if( $stock{0} == '6' ){
			$xstock = "sh".$stock;
		}else{
			$xstock = "sz".$stock;
		}
		$link =sprintf("http://market.finance.sina.com.cn/downxls.php?date=%s&symbol=%s",$ymd,$xstock);
		$fn = sprintf("tick/%s/%s.csv",$ymd,$stock);
		$fe = sprintf("tick/%s/%s.err",$ymd,$stock);
		if( file_exists($fn) ){
			continue ;
		}
		if( file_exists($fe) ){
			$ect++;
			continue ;
		}
		//! 并发数不超限
		run_script_wait($pids,8);
		//!生成每日的信号
		$pid = pcntl_fork();
		if( $pid > 0 ){
			//! 管理进程压进程号
			array_push($pids,$pid);
		}else
		if( $pid < 0 ){
			//! fork 出错，等一秒再试，十次失败退出
			sleep(1);
			$ec ++;
			continue;
		}else
		if( $pid == 0 ){
			download($fn,$fe,$link);
			exit(0);
		}
	}
	/*
	if($ect == $all ){
		//! 有可能是节假日吧
		break;
	}
	*/
	printf("\n",$cnt);
}


function download($fn,$fe,$link){
	$fc = array();
	$retry=3;
	while( sizeof($fc) == 0 ){
		$retry -- ;
		if( $retry <= 0 ){
			return ;
		}
		$fc = @file_get_contents($link);
		if( strstr($fc,"javascript") != "" ){
			break;
		}
		if( strlen($fc) > 100 ){
			file_put_contents($fn,$fc);
			return;
		}
		sleep(1);
	}
	file_put_contents($fe,$fc);
}


function run_script_wait(&$pids,$threads){
	while( sizeof($pids) >= $threads ){
		foreach($pids as $index => $pid){
			if( pcntl_wait($pid,WNOHANG) > 0 ){
				unset($pids[$index]);
				break;
			}
		}
	}
}

function run_script_wait_all(&$pids){
	while( sizeof($pids) > 0  ){
		printf("wait %d process\n",sizeof($pids));
		$remove = array();
		foreach($pids as $index => $pid){
			if( pcntl_wait($pid,WNOHANG) > 0 ){
				array_push($remove,$index);
			}
		}
		foreach($remove as $index){
			unset($pids[$index]);
		}
		if( sizeof($pids) > 0 ){
			sleep(1);
		}
	}
}



