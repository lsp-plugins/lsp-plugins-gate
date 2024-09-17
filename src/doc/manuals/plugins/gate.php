<?php
	plugin_header();
	
	$sc     =   (strpos($PAGE, 'sc_') === 0);
	$m      =   (strpos($PAGE, '_mono') > 0) ? 'm' : (
			    (strpos($PAGE, '_stereo') > 0) ? 's' : (
				(strpos($PAGE, '_lr') > 0) ? 'lr' : (
				(strpos($PAGE, '_ms') > 0) ? 'ms' : '?'
				)));
	$sm     =   ($m == 'ms') ? ' M, S' : (($m != 'm') ? ' L, R' : '');
	$cc     =   ($m == 'm') ? 'mono' : 'stereo';
?>

<p>
	This plugin performs gating of <?= $cc ?> input signal<?php 
	if ($m == 'ms') echo " in Mid-Side mode";
	elseif ($m == 'lr') echo " by applying individual processing to left and right channels separately";
	?>. Flexible sidechain-control configuration <?php
		if ($sc)
			echo " and additional sidechain input" . (($m == 'm') ? '' : 's') . " are ";
		else
			echo " is";
	?>provided. Additional <b>Hysteresis</b> curve is available to provide accurate control of the fading of the signal.
	Also additional	dry/wet control allows to mix processed and unprocessed signal together. 
</p>
<?php if ($m == 's') { ?>
<p>Additionally, <b>Stereo split mode</b> allows to apply processing to the left and right channels independently while
keeping the same settings for the left and right channels.</p>
<?php } ?>
<p><b>Controls:</b></p>
<ul>
	<li>
		<b>Bypass</b> - bypass switch, when turned on (led indicator is shining), the plugin bypasses signal.
	</li>
	<li><b>Pause</b> - pauses any updates of the gate graph.</li>
	<li><b>Clear</b> - clears all graphs.</li>
	<?php if ($m == 's') { ?>
		<li><b>Stereo Split</b> - enables independent compression of left and right channels.</li>
	<?php } elseif ($m == 'ms') { ?>
		<li><b>MS Listen</b> - passes mid-side signal to the output of gate instead of stereo signal.</li>
	<?php } ?>
	<li><b>Gain<?= $sm ?></b> - enables drawing of gain amplification line and corresponding amplification meter.</li>
	<li><b>SC<?= $sm ?></b> - enables drawing of sidechain input graph and corresponding level meter.</li>
	<li><b>Env<?= $sm ?></b> - enables drawing of gate's envelope graph and corresponding level meter.</li>
	<li><b>In<?= $sm ?></b> - enables drawing of gate's input signal graph and corresponding level meter.</li>
	<li><b>Out<?= $sm ?></b> - enables drawing of gate's output signal graph and corresponding level meter.</li>
	<li><b>Link</b> - the name of the shared memory link to pass control/match signal.</li>
</ul>
<p><b>'Sidechain' section:</b></p>
<ul>
	<li><b>Preamp</b> - pre-amplification of the sidechain signal.</li>
	<li><b>Reactivity</b> - reactivity of the sidechain signal.</li>
	<li><b>Lookahead</b> - look-ahead time of the sidechain relative to the input signal.</li>
	<li><b>Setup</b> - Sidechain configuration, available values:</li>
	<ul>
		<li><b>Internal</b> - sidechain input is connected to gate's input.</li>
		<?php if ($sc) { ?>
			<li><b>External</b> - sidechain signal is taken from additional (external) sidechain inputs of plugin.</li>
		<?php }?>
		<li><b>Link</b> - sidechain input is passed by shared memory link.</li>
		<li><b>Peak</b> - peak mode.</li>
		<li><b>RMS</b> - Root Mean Square (SMA) of the input signal.</li>
		<li><b>LPF</b> - input signal processed by recursive 1-pole Low-Pass Filter (LPF).</li>
		<li><b>SMA</b> - input signal processed by Simple Moving Average (SMA) filter.</li>
		<?php if ($m != 'm') { ?>
			<li><b>Middle</b> - middle part of signal is used for sidechain processing.</li>
			<li><b>Side</b> - side part of signal is used for sidechain processing.</li>
			<li><b>Left</b> - only left channel is used for sidechain processing.</li>
			<li><b>Right</b> - only right channel is used for sidechain processing.</li>
			<li><b>Min</b> - the absolute minimum value is taken from stereo input.</li>
			<li><b>Max</b> - the absolute maximum value is taken from stereo input.</li>
		<?php } ?>
		<?php if ($m == 's') { ?>
			<li><b>Left/Right</b> - left and right channels are being processed using respectively the left and right sidechain channels in stereo split mode.</li>
			<li><b>Right/Left</b> - left and right channels are being processed using respectively the right and left sidechain channels in stereo split mode.</li>
			<li><b>Mid/Side</b> - left and right channels are being processed using respectively the middle and side parts of sidechain signal in stereo split mode.</li>
			<li><b>Side/Mid</b> - left and right channels are being processed using respectively the side and middle parts of sidechain signal in stereo split mode.</li>
		<?php } ?>
	</ul>
	<li><b>Listen</b> - allows to listen the <b>processed</b> sidechain signal.</li>
	<li><b>LPF</b> - allows to set up slope and cut-off frequency for the low-pass filter applied to sidechain signal.</li>
	<li><b>HPF</b> - allows to set up slope and cut-off frequency for the high-pass filter applied to sidechain signal.</li>
</ul>
<p><b>'Gate' section:</b></p>
<ul>
	<li><b>Reduction</b> - the amount of gain applied to the input signal when the gate is closed (if negative) or open (if positive).</li>
	<li><b>Makeup</b> - additional amplification gain after processing stage.</li>
	<li><b>Attack</b> - attack time of the gate.</li>
	<li><b>Release</b> - release time of the gate.</li>
	<li><b>Curve</b> - Basic gate curve characteristics:</li>
	<ul>
		<li><b>Thresh</b> - threshold, the level of input signal at which the gate becomes open.</li>
		<li><b>Zone</b> - the size of the transition zone below the threshold.</li>
	</ul>
	<li><b>Hysteresis <?= $sm ?></b> - enables additional hysteresis curve:</li>
	<ul>
		<li><b>Thresh</b> - threshold (relative to basic curve), the level of input signal at which the gate starts to close.</li>
		<li><b>Zone</b> - the size of the transition zone below the hysteresis threshold.</li>
	</ul>
	<li><b>Hold</b> - the time period the envelope holds it's maximum value before starting the release.</li>
	<li><b>Mix Dry</b> - the amount of dry (unprocessed) signal.</li>
	<li><b>Mix Wet</b> - the amount of wet (processed) signal.</li>
	<li><b>Mix Dry/Wet</b> - the knob that controls this balance between mixed dry and wet signal (see <b>Mix Dry</b> and <b>Mix Wet</b>) and the dry (unprocessed) signal.</li>
</ul>

<p><b>'Signal' section:</b></p>
<ul>
	<li><b>Input</b> - overall input gain.</li>
	<li><b>Output</b> - overall output gain.</li>
</ul>