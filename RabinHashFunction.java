package pt.tumba.parser;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.net.URL;

/**
 * We compute the checksum using Broder s implementation of
 * Rabin s fingerprinting algorithm. Fingerprints offer 
 * provably strong probabilistic guarantees that two 
 * different strings will not have the same fingerprint. 
 * Other checksum algorithms, such as MD5 and SHA, do not 
 * offer such provable guarantees, and are also more 
 * expensive to compute than Rabin fingerprint.
 *
 * A disadvantage is that these faster functions are 
 * efficiently invertible (that is, one can easily build an 
 * URL that hashes to a particular location), a fact that  
 * might be used by malicious users to nefarious purposes.
 *
 * Using the Rabin's fingerprinting function, the probability of
 * collision of two strings s1 and s2 can be bounded (in a adversarial
 * model for s1 and s2) by max(|s1|,|s2|)/2**(l-1), where |s1| is the 
 * length of the string s1 in bits.
 * 
 * The advantage of choosing Rabin fingerprints (which are based on random
 * irreducible polynomials) rather than some arbitrary hash function is that
 * their probability of collision os well understood. Furthermore Rabin 
 * fingerprints can be computed very efficiently in software and we can
 * take advantage of their algebraic properties when we compute the
 * fingerprints of "sliding windows".
 *
 * M. O. Rabin
 * Fingerprinting by random polynomials.
 * Center for Research in Computing Technology
 * Harvard University Report TR-15-81
 * 1981
 * 
 * A. Z. Broder
 * Some applications of Rabin's fingerprinting method
 * In R.Capicelli, A. De Santis and U. Vaccaro editors
 * Sequences II:Methods in Communications, Security, and Computer Science
 * pages 143-152
 * Springer-Verlag
 * 1993
 *
 */
public final class RabinHashFunction implements Serializable {

	private final static int P_DEGREE = 64;
	private final static int READ_BUFFER_SIZE = 2048;
	private final static int X_P_DEGREE = 1 << (P_DEGREE - 1);

	public static void main(String args[]) {
		RabinHashFunction h = new RabinHashFunction();
		System.out.println(h.hash(args[0]));
	}

	private final byte[] buffer;

	private long POLY = Long.decode("0x0060034000F0D50A").longValue();

	private final long[] table32, table40, table48, table54;
	private final long[] table62, table70, table78, table84;

	/**
	 *  Constructor for the RabinHashFunction64 object
	 *
	 *@param  P  Description of the Parameter
	 */
	public RabinHashFunction() {
		table32 = new long[256];
		table40 = new long[256];
		table48 = new long[256];
		table54 = new long[256];
		table62 = new long[256];
		table70 = new long[256];
		table78 = new long[256];
		table84 = new long[256];
		buffer = new byte[READ_BUFFER_SIZE];
		long[] mods = new long[P_DEGREE];
		mods[0] = POLY;
		for (int i = 0; i < 256; i++) {
			table32[i] = 0;
			table40[i] = 0;
			table48[i] = 0;
			table54[i] = 0;
			table62[i] = 0;
			table70[i] = 0;
			table78[i] = 0;
			table84[i] = 0;
		}
		for (int i = 1; i < P_DEGREE; i++) {
			mods[i] = mods[i - 1] << 1;
			if ((mods[i - 1] & X_P_DEGREE) != 0) {
				mods[i] = mods[i] ^ POLY;
			}
		}
		for (int i = 0; i < 256; i++) {
			long c = i;
			for (int j = 0; j < 8 && c != 0; j++) {
				if ((c & 1) != 0) {
					table32[i] = table32[i] ^ mods[j];
					table40[i] = table40[i] ^ mods[j + 8];
					table48[i] = table48[i] ^ mods[j + 16];
					table54[i] = table54[i] ^ mods[j + 24];
					table62[i] = table62[i] ^ mods[j + 32];
					table70[i] = table70[i] ^ mods[j + 40];
					table78[i] = table78[i] ^ mods[j + 48];
					table84[i] = table84[i] ^ mods[j + 56];
				}
				c >>>= 1;
			}
		}
	}

	/**
	 *  Return the Rabin hash value of an array of bytes.
	 *
	 *@param  A  the array of bytes
	 *@return    the hash value
	 */
	public long hash(byte[] A) {
		return hash(A, 0, A.length, 0);
	}

	/**
	 *  Description of the Method
	 *
	 *@param  A       Description of the Parameter
	 *@param  offset  Description of the Parameter
	 *@param  length  Description of the Parameter
	 *@param  w       Description of the Parameter
	 *@return         Description of the Return Value
	 */
	private long hash(byte[] A, int offset, int length, long ws) {
		long w = ws;
		int start = length % 8;
		for (int s = offset; s < offset + start; s++) {
			w = (w << 8) ^ (A[s] & 0xFF);
		}
		for (int s = offset + start; s < length + offset; s += 8) {
			w =
				table32[(int) (w & 0xFF)]
					^ table40[(int) ((w >>> 8) & 0xFF)]
					^ table48[(int) ((w >>> 16) & 0xFF)]
					^ table54[(int) ((w >>> 24) & 0xFF)]
					^ table62[(int) ((w >>> 32) & 0xFF)]
					^ table70[(int) ((w >>> 40) & 0xFF)]
					^ table78[(int) ((w >>> 48) & 0xFF)]
					^ table84[(int) ((w >>> 56) & 0xFF)]
					^ (long) (A[s] << 56)
					^ (long) (A[s + 1] << 48)
					^ (long) (A[s + 2] << 40)
					^ (long) (A[s + 3] << 32)
					^ (long) (A[s + 4] << 24)
					^ (long) (A[s + 5] << 16)
					^ (long) (A[s + 6] << 8)
					^ (long) (A[s + 7]);
		}
		return w;
	}

	/**
	 *  Return the Rabin hash value of an array of chars.
	 *
	 *@param  A  the array of chars
	 *@return    the hash value
	 */
	public long hash(char[] A) {
		long w = 0;
		int start = A.length % 4;
		for (int s = 0; s < start; s++) {
			w = (w << 16) ^ (A[s] & 0xFFFF);
		}
		for (int s = start; s < A.length; s += 4) {
			w =
				table32[(int) (w & 0xFF)]
					^ table40[(int) ((w >>> 8) & 0xFF)]
					^ table48[(int) ((w >>> 16) & 0xFF)]
					^ table54[(int) ((w >>> 24) & 0xFF)]
					^ table62[(int) ((w >>> 32) & 0xFF)]
					^ table70[(int) ((w >>> 40) & 0xFF)]
					^ table78[(int) ((w >>> 48) & 0xFF)]
					^ table84[(int) ((w >>> 56) & 0xFF)]
					^ ((long) (A[s] & 0xFFFF) << 48)
					^ ((long) (A[s + 1] & 0xFFFF) << 32)
					^ ((long) (A[s + 2] & 0xFFFF) << 16)
					^ ((long) (A[s + 3] & 0xFFFF));
		}
		return w;
	}

	/**
	 *  Computes the Rabin hash value of the contents of a file.
	 *
	 *@param  f                       the file to be hashed
	 *@return                         the hash value of the file
	 *@throws  FileNotFoundException  if the file cannot be found
	 *@throws  IOException            if an error occurs while reading the file
	 */
	public long hash(File f) throws FileNotFoundException, IOException {
		FileInputStream fis = new FileInputStream(f);
		try {
			return hash(fis);
		} finally {
			fis.close();
		}
	}

	/**
	 *  Computes the Rabin hash value of the data from an <code>InputStream</code>.
	 *
	 *@param  is            the InputStream to hash
	 *@return               the hash value of the data from the InputStream
	 *@throws  IOException  if an error occurs while reading from the
	 *      InputStream
	 */
	public long hash(InputStream is) throws IOException {
		long hashValue = 0;
		int bytesRead;
		synchronized (buffer) {
			while ((bytesRead = is.read(buffer)) > 0) {
				hashValue = hash(buffer, 0, bytesRead, hashValue);
			}
		}
		return hashValue;
	}

	/**
	 *  Returns the Rabin hash value of an array of integers. This method is the
	 *  most efficient of all the hash methods, so it should be used when
	 *  possible.
	 *
	 *@param  A  array of integers
	 *@return    the hash value
	 */
	public long hash(int[] A) {
		long w = 0;
		int start = 0;
		if (A.length % 2 == 1) {
			w = A[0] & 0xFFFFFFFF;
			start = 1;
		}
		for (int s = start; s < A.length; s += 2) {
			w =
				table32[(int) (w & 0xFF)]
					^ table40[(int) ((w >>> 8) & 0xFF)]
					^ table48[(int) ((w >>> 16) & 0xFF)]
					^ table54[(int) ((w >>> 24) & 0xFF)]
					^ table62[(int) ((w >>> 32) & 0xFF)]
					^ table70[(int) ((w >>> 40) & 0xFF)]
					^ table78[(int) ((w >>> 48) & 0xFF)]
					^ table84[(int) ((w >>> 56) & 0xFF)]
					^ ((long) (A[s] & 0xFFFFFFFF) << 32)
					^ (long) (A[s + 1] & 0xFFFFFFFF);
		}
		return w;
	}

	/**
	 *  Returns the Rabin hash value of an array of longs. This method is the
	 *  most efficient of all the hash methods, so it should be used when
	 *  possible.
	 *
	 *@param  A  array of integers
	 *@return    the hash value
	 */
	public long hash(long[] A) {
		long w = 0;
		for (int s = 0; s < A.length; s++) {
			w =
				table32[(int) (w & 0xFF)]
					^ table40[(int) ((w >>> 8) & 0xFF)]
					^ table48[(int) ((w >>> 16) & 0xFF)]
					^ table54[(int) ((w >>> 24) & 0xFF)]
					^ table62[(int) ((w >>> 32) & 0xFF)]
					^ table70[(int) ((w >>> 40) & 0xFF)]
					^ table78[(int) ((w >>> 48) & 0xFF)]
					^ table84[(int) ((w >>> 56) & 0xFF)]
					^ (A[s]);
		}
		return w;
	}

	/**
	 *  Description of the Method
	 *
	 *@param  obj              Description of the Parameter
	 *@return                  Description of the Return Value
	 *@exception  IOException  Description of the Exception
	 */
	public long hash(Object obj) throws IOException {
		return hash((Serializable) obj);
	}

	/**
	 *  Returns the Rabin hash value of a serializable object.
	 *
	 *@param  obj           the object to be hashed
	 *@return               the hash value
	 *@throws  IOException  if serialization fails
	 */
	public long hash(Serializable obj) throws IOException {
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		ObjectOutputStream oos = null;
		try {
			oos = new ObjectOutputStream(baos);
			oos.writeObject(obj);
			return hash(baos.toByteArray());
		} finally {
			oos.close();
			baos.close();
			oos = null;
			baos = null;
		}
	}

	/**
	 *  Computes the Rabin hash value of a String.
	 *
	 *@param  s  the string to be hashed
	 *@return    the hash value
	 */
	public long hash(String s) {
		return hash(s.toCharArray());
	}

	/**
	 *  Computes the Rabin hash value of the contents of a file, specified by
	 *  URL.
	 *
	 *@param  url           the URL of the file to be hashed
	 *@return               the hash value of the file
	 *@throws  IOException  if an error occurs while reading from the URL
	 */
	public long hash(URL url) throws IOException {
		InputStream is = url.openStream();
		try {
			return hash(is);
		} finally {
			is.close();
		}
	}

}
