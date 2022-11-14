.. _l1as:

Level 1 Archiving Suite
=======================

Add more about information.

The FORCE Level 1 Archiving Suite (FORCE L1AS) assists in organizing and maintaining a clean and consistent Level 1 data pool, as well as downloading of Sentinel-2 data.
It is attempted to reduce redundancy and supports versioning, e.g. by removing old data if new processing versions are available.
In addition, FORCE L1AS assists in building and updating the file queues needed for :ref:`l2ps`:


.. image:: L1AS.jpg

**Figure.** FORCE Level 1 Archiving Suite (L1AS) workflow. 

.. note::
   As of ``FORCE v. 3.5`` :ref:`level1-csd` replaces :ref:`level1-landsat` and :ref:`level1-sentinel2`.
   The deprecated tools will be removed in a future FORCE version. 
   We recommend to change your processes accordingly.

On successful ingestion, the image is appended to a :ref:`queue`, which controls Level 2 processing. 
The file queue is a text file that holds the full path to the image, as well as a processing-state flag. 
This flag is either ``QUEUED`` or ``DONE``, which means that it is enqueued for Level 2 processing or was already processed and will be ignored next time.


+-------------------+-----------------------+-------------------------+--------------+
+ :ref:`level1-csd` + :ref:`level1-landsat` + :ref:`level1-sentinel2` + :ref:`queue` +
+-------------------+-----------------------+-------------------------+--------------+


.. toctree::
   :maxdepth: 1
   :hidden:

   level1-csd.rst
   level1-landsat.rst
   level1-sentinel2.rst
   queue.rst

   